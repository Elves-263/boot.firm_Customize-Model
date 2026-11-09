#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include "csvc.h"
#include "menus/lang.h"
#include "plugin.h"
#include "ifile.h"
#include "ifile.h"
#include "utils.h"
#include "menu.h"
#include "draw.h"


static FS_DirectoryEntry   g_entries[10];

static char        g_path[256];
static const char *g_dirPath = "/luma/plugins/%016llX";
static const char *g_defaultPath = "/luma/plugins/default.3gx";


void        gamePatchFunc(void);

static Result   OpenFile(IFile *file, const char *path);



static bool     IsPluginLoadable(const char *path)
{
    IFile   f;
    bool    ok;

    if (R_FAILED(OpenFile(&f, path)))
        return false;

    ok = R_SUCCEEDED(Check_3gx_Magic(&f));
    IFile_Close(&f);

    return ok;
}

static u32     strlen16(const u16 *str)
{
    if (!str) return 0;

    const u16 *strEnd = str;

    while (*strEnd) ++strEnd;

    return strEnd - str;
}

#define MAX_PLUGIN_ENTRIES 32
#define SELECTION_FILENAME ".selected3gx"
#define COLOR_GRAY RGB565(0x08, 0x10, 0x08)

static char     g_pluginEntries[MAX_PLUGIN_ENTRIES][256];
static bool     g_pluginEntryOk[MAX_PLUGIN_ENTRIES];
static u32      g_pluginEntryCount;
static u32      g_pluginEntryOkCount;
static char     g_pluginDir[300];
static u64      g_pluginHash;

static Result   EnumeratePluginFiles(u64 tid)
{
    u32                 entriesNb = 0;
    Handle              dir = 0;
    Result              res;
    FS_Archive          sdmcArchive = 0;
    FS_DirectoryEntry * entries = g_entries;
    static const u16 *  validExtension = u".3gx";

    g_pluginEntryCount = 0;
    g_pluginEntryOkCount = 0;

    memset(entries, 0, sizeof(g_entries));
    sprintf(g_path, g_dirPath, tid);

    if (R_FAILED((res = FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, "")))))
        return res;

    if (R_FAILED((res = FSUSER_OpenDirectory(&dir, sdmcArchive, fsMakePath(PATH_ASCII, g_path)))))
    {
        FSUSER_CloseArchive(sdmcArchive);
        return res;
    }

    strcat(g_path, "/");
    strcpy(g_pluginDir, g_path);

    while (R_SUCCEEDED(FSDIR_Read(dir, &entriesNb, 10, entries)) && entriesNb != 0)
    {
        for (u32 i = 0; i < entriesNb && g_pluginEntryCount < MAX_PLUGIN_ENTRIES; ++i)
        {
            FS_DirectoryEntry *entry = &entries[i];

            if (entry->attributes & 1)
                continue;

            u32 size = strlen16(entry->name);
            if (size <= 4)
                continue;

            if (entry->name[0] == u'.')
                continue;

            u16 *fileExt = entry->name + size - 4;
            if (memcmp(fileExt, validExtension, 4 * sizeof(u16)))
                continue;

            int units = utf16_to_utf8((u8 *)g_pluginEntries[g_pluginEntryCount],
                                      entry->name, sizeof(g_pluginEntries[0]) - 1);
            if (units < 0 || (u32)units >= sizeof(g_pluginEntries[0]))
                continue;

            g_pluginEntries[g_pluginEntryCount][units] = 0;
            ++g_pluginEntryCount;
        }
    }

    FSDIR_Close(dir);
    FSUSER_CloseArchive(sdmcArchive);

    for (u32 i = 1; i < g_pluginEntryCount; ++i)
    {
        char    tmp[256];
        s32     j = (s32)i - 1;

        strcpy(tmp, g_pluginEntries[i]);
        while (j >= 0 && strcmp(g_pluginEntries[j], tmp) > 0)
        {
            strcpy(g_pluginEntries[j + 1], g_pluginEntries[j]);
            --j;
        }
        strcpy(g_pluginEntries[j + 1], tmp);
    }

    for (u32 i = 0; i < g_pluginEntryCount; ++i)
    {
        char name[sizeof(g_pluginEntries[0])];
        char path[sizeof(g_path) + sizeof(name)];

        strncpy(name, g_pluginEntries[i], sizeof(name) - 1);
        name[sizeof(name) - 1] = 0;

        sprintf(path, "%s%s", g_path, name);
        g_pluginEntryOk[i] = IsPluginLoadable(path);
        if (g_pluginEntryOk[i])
            ++g_pluginEntryOkCount;
    }

    return 0;
}

static bool     LoadSelection(char *outName, u32 outSize)
{
    char    path[300];
    IFile   f;
    u64     total = 0;
    bool    ok = false;

    sprintf(path, "%s%s", g_path, SELECTION_FILENAME);

    if (R_SUCCEEDED(OpenFile(&f, path)))
    {
        memset(outName, 0, outSize);
        if (R_SUCCEEDED(IFile_Read(&f, &total, outName, outSize - 1)) && total > 0)
        {
            for (u32 i = 0; i < (u32)total; ++i)
            {
                if (outName[i] == '\r' || outName[i] == '\n')
                {
                    outName[i] = 0;
                    break;
                }
            }
            ok = outName[0] != 0;
        }
        IFile_Close(&f);
    }

    return ok;
}

static void     SaveSelection(const char *name)
{
    char    path[300];
    IFile   f;
    u64     total;

    sprintf(path, "%s%s", g_path, SELECTION_FILENAME);

    if (R_SUCCEEDED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                               fsMakePath(PATH_ASCII, path), FS_OPEN_CREATE | FS_OPEN_WRITE)))
    {
        IFile_Write(&f, &total, (void *)name, strlen(name), FS_WRITE_FLUSH);
        IFile_Close(&f);
    }
}

static void     ClearSelection(void)
{
    char        path[300];
    FS_Archive  sdmcArchive = 0;

    sprintf(path, "%s%s", g_path, SELECTION_FILENAME);

    if (R_SUCCEEDED(FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
    {
        FSUSER_DeleteFile(sdmcArchive, fsMakePath(PATH_ASCII, path));
        FSUSER_CloseArchive(sdmcArchive);
    }
}

static s32      NextLoadableEntry(s32 from, s32 step)
{
    if (g_pluginEntryCount == 0)
        return -1;

    for (u32 n = 0; n < g_pluginEntryCount; ++n)
    {
        from = (from + step + (s32)g_pluginEntryCount) % (s32)g_pluginEntryCount;
        if (g_pluginEntryOk[from])
            return from;
    }
    return -1;
}

static bool     ShowPluginSelector(char *outName, u32 outSize)
{
    s32     cursor;
    u32     keys;
    char    pinned[256];
    bool    havePinned;

    if (g_pluginEntryOkCount == 0)
        return false;

    cursor = g_pluginEntryOk[0] ? 0 : NextLoadableEntry(0, 1);
    if (cursor < 0)
        return false;

    havePinned = LoadSelection(pinned, sizeof(pinned));

    menuEnter();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Plugin selector");
        Draw_DrawString(10, 30, COLOR_WHITE, Lang(STR_A_USE_X_PIN_UNPIN_B_NONE));

        u32 posY = 50;
        for (u32 i = 0; i < g_pluginEntryCount; ++i)
        {
            char    name[sizeof(g_pluginEntries[0])];
            char    line[sizeof(name) + 4];
            u32     color;

            strncpy(name, g_pluginEntries[i], sizeof(name) - 1);
            name[sizeof(name) - 1] = 0;

            if (!g_pluginEntryOk[i])
                color = COLOR_GRAY;
            else if (havePinned && strcmp(name, pinned) == 0)
                color = COLOR_YELLOW;
            else
                color = COLOR_WHITE;

            sprintf(line, "%s%s", ((s32)i == cursor) ? "> " : "  ", name);
            Draw_DrawString(10, posY, color, line);
            posY += SPACING_Y;
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        keys = waitInput();

        if (menuShouldExit)
            break;

        if (keys & KEY_DOWN)
            cursor = NextLoadableEntry(cursor, 1);
        else if (keys & KEY_UP)
            cursor = NextLoadableEntry(cursor, -1);
        else if (keys & KEY_A)
        {
            if (cursor < 0)
                continue;
            strncpy(outName, g_pluginEntries[cursor], outSize - 1);
            outName[outSize - 1] = 0;
            menuLeave();
            return true;
        }
        else if (keys & KEY_X)
        {
            if (cursor < 0)
                continue;
            if (havePinned && strcmp(g_pluginEntries[cursor], pinned) == 0)
            {
                ClearSelection();
                havePinned = false;
                pinned[0] = 0;
            }
            else
            {
                strncpy(pinned, g_pluginEntries[cursor], sizeof(pinned) - 1);
                pinned[sizeof(pinned) - 1] = 0;
                SaveSelection(pinned);
                havePinned = true;
            }
        }
        else if (keys & KEY_B)
        {
            menuLeave();
            return false;
        }
    }
    while (!menuShouldExit);

    menuLeave();
    return false;
}

static Result   FindPluginFile(u64 tid)
{
    char    chosen[256];
    char    saved[256];
    bool    haveChoice = false;
    u32     heldKeys;

    if (R_FAILED(EnumeratePluginFiles(tid)) || g_pluginEntryCount == 0)
        return MAKERESULT(28, 4, 0, 1018);

    if (g_pluginEntryOkCount == 0)
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, 2);

    heldKeys = HID_PAD;

    if (g_pluginEntryOkCount == 1 && g_pluginEntryCount == 1)
    {
        strcpy(chosen, g_pluginEntries[0]);
        haveChoice = true;
    }
    else if (!(heldKeys & KEY_Y) && LoadSelection(saved, sizeof(saved)))
    {
        bool stillUsable = false;

        for (u32 i = 0; i < g_pluginEntryCount; ++i)
        {
            if (strcmp(g_pluginEntries[i], saved) == 0)
            {
                stillUsable = g_pluginEntryOk[i];
                break;
            }
        }

        if (stillUsable)
        {
            strcpy(chosen, saved);
            haveChoice = true;
        }
        else
            haveChoice = ShowPluginSelector(chosen, sizeof(chosen));
    }
    else
        haveChoice = ShowPluginSelector(chosen, sizeof(chosen));

    if (!haveChoice)
        return MAKERESULT(28, 4, 0, 1018);

    strcat(g_path, chosen);
    PluginLoaderCtx.pluginPath = g_path;

    return 0;
}

typedef struct DestructiveKeyword
{
    const char *text;
    u32         weight;
} DestructiveKeyword;

static const DestructiveKeyword g_destructiveKeywords[] =
{
    { "boot9strap",   40 },
    { "boot.firm",    40 },
    { "/private",     25 },
    { "/cias",        20 },
    { "/gm9",         20 },
    { "/fbi",         20 },
    { "Nintendo 3DS", 15 },
    { "/DCIM",        10 },
    { "/JKSV",        10 },
    { "/Splashes",    10 },
};
#define DESTRUCTIVE_KEYWORD_COUNT (sizeof(g_destructiveKeywords) / sizeof(g_destructiveKeywords[0]))

static char lowerAscii(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static s32 memstrCaseless(const char *hay, u32 hayLen, const char *needle, u32 needleLen)
{
    if (needleLen == 0 || hayLen < needleLen)
        return -1;

    for (u32 i = 0; i + needleLen <= hayLen; ++i)
    {
        u32 j = 0;

        while (j < needleLen && lowerAscii(hay[i + j]) == lowerAscii(needle[j]))
            ++j;

        if (j == needleLen)
            return (s32)i;
    }

    return -1;
}

#define APPROVED_CACHE_SIZE 32

static u64      g_approvedHashes[APPROVED_CACHE_SIZE];
static u32      g_approvedCount;

static bool     IsHashApproved(u64 hash)
{
    for (u32 i = 0; i < g_approvedCount; ++i)
        if (g_approvedHashes[i] == hash)
            return true;

    return false;
}

static void     RememberApproval(u64 hash)
{
    if (IsHashApproved(hash))
        return;

    if (g_approvedCount < APPROVED_CACHE_SIZE)
    {
        g_approvedHashes[g_approvedCount++] = hash;
        return;
    }

    for (u32 i = 1; i < APPROVED_CACHE_SIZE; ++i)
        g_approvedHashes[i - 1] = g_approvedHashes[i];

    g_approvedHashes[APPROVED_CACHE_SIZE - 1] = hash;
}

static void     DrawMatchDetails(IFile *plugin, const bool *found, const u32 *offsets)
{
    static char raw[32];
    char        text[25];
    u64         total;
    u32         posY = 30;

    Draw_DrawString(10, 10, COLOR_TITLE, Lang(STR_MATCHED_LOCATIONS));

    for (u32 i = 0; i < DESTRUCTIVE_KEYWORD_COUNT; ++i)
    {
        u32 n = 0;

        if (!found[i])
            continue;

        u32 base = offsets[i] > 8 ? offsets[i] - 8 : 0;

        plugin->pos = base;
        if (R_SUCCEEDED(IFile_Read(plugin, &total, raw, 24)))
        {
            u32 got = (u32)total > 24 ? 24 : (u32)total;

            for (n = 0; n < got; ++n)
            {
                char c = raw[n];
                text[n] = (c >= 0x20 && c < 0x7F) ? c : '.';
            }
        }
        text[n] = 0;

        posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "%08lx %-12s %s\n",
                                        offsets[i], g_destructiveKeywords[i].text, text);
    }

    plugin->pos = 0;
}

static bool     CheckForDestructivePlugin(IFile *plugin, u64 fileSize)
{
    static char buffer[2000];
    u64         offset = 0;
    u64         total;
    u64         hash = 0xCBF29CE484222325ULL;
    u64         hashed = 0;
    bool        found[DESTRUCTIVE_KEYWORD_COUNT] = { false };
    u32         matchOffset[DESTRUCTIVE_KEYWORD_COUNT] = { 0 };
    u32         keywordLen[DESTRUCTIVE_KEYWORD_COUNT];
    bool        readFailed = false;
    u32         maxLen = 1;

    for (u32 i = 0; i < DESTRUCTIVE_KEYWORD_COUNT; ++i)
    {
        keywordLen[i] = (u32)strlen(g_destructiveKeywords[i].text);
        if (keywordLen[i] > maxLen)
            maxLen = keywordLen[i];
    }

    if (maxLen >= sizeof(buffer))
        maxLen = sizeof(buffer) - 1;

    while (offset < fileSize)
    {
        u32 chunk = (u32)((fileSize - offset) > sizeof(buffer) ? sizeof(buffer) : (fileSize - offset));

        plugin->pos = offset;
        if (R_FAILED(IFile_Read(plugin, &total, buffer, chunk)) || (u32)total != chunk)
        {
            readFailed = true;
            break;
        }

        for (u32 i = 0; i < DESTRUCTIVE_KEYWORD_COUNT; ++i)
        {
            if (found[i])
                continue;

            s32 at = memstrCaseless(buffer, chunk, g_destructiveKeywords[i].text, keywordLen[i]);
            if (at >= 0)
            {
                found[i] = true;
                matchOffset[i] = (u32)(offset + (u32)at);
            }
        }

        u32 fresh = (offset >= hashed) ? 0 : (u32)(hashed - offset);
        for (u32 i = fresh; i < chunk; ++i)
        {
            hash ^= (u8)buffer[i];
            hash *= 0x100000001B3ULL;
        }
        if (offset + chunk > hashed)
            hashed = offset + chunk;

        offset += (chunk > maxLen && offset + chunk < fileSize)
            ? chunk - (maxLen - 1) : chunk;
    }

    plugin->pos = 0;
    g_pluginHash = hash;

    u32 risk = 0;
    for (u32 i = 0; i < DESTRUCTIVE_KEYWORD_COUNT; ++i)
        if (found[i])
            risk += g_destructiveKeywords[i].weight;

    if (risk > 100)
        risk = 100;

    if (risk == 0 && !readFailed)
        return true;

    if (!readFailed && IsHashApproved(hash))
        return true;

    const char  *warningText = risk <= 20 ? "little dangerous" : risk <= 40 ? "moderately dangerous" : "very dangerous";
    char        textBuf[64];
    u32         keys;
    bool        result = true;
    bool        details = false;

    if (readFailed)
        sprintf(textBuf, "Could not scan the whole file");
    else
        sprintf(textBuf, "Risk : %u%% (%s)", (unsigned)risk, warningText);

    char matched[256];
    matched[0] = 0;
    for (u32 i = 0; i < DESTRUCTIVE_KEYWORD_COUNT; ++i)
    {
        if (found[i])
        {
            if (matched[0] != 0 && (u32)(strlen(matched) + 2) < (u32)sizeof(matched))
                strcat(matched, ", ");
            if ((u32)(strlen(matched) + strlen(g_destructiveKeywords[i].text) + 1) < (u32)sizeof(matched))
                strcat(matched, g_destructiveKeywords[i].text);
        }
    }

    if (matched[0] == 0)
        strcat(matched, "(none)");

    menuEnter();

    do
    {
        Draw_Lock();
        Draw_ClearFramebuffer();

        if (details)
        {
            DrawMatchDetails(plugin, found, matchOffset);
            Draw_DrawString(10, 205, COLOR_WHITE, Lang(STR_X_BACK_TO_THE_WARNING));
        }
        else
        {
            Draw_DrawString(10, 10, COLOR_RED, "--- KEIKOKU / WARNING ---");
            u32 posY = readFailed
                ? Draw_DrawBilingual(30, 30, COLOR_WHITE, "Kono 3gx wa saigo made kensa dekimasen",
                                     "This 3gx could not be fully checked")
                : Draw_DrawBilingual(30, 30, COLOR_WHITE, "Kono 3gx wa hakaiteki kamo shiremasen",
                                     "This 3gx may be a destructive plugin");
            posY = Draw_DrawString(30, posY + SPACING_Y, COLOR_WHITE, textBuf);
            posY = Draw_DrawString(30, posY + SPACING_Y, COLOR_WHITE, "Matched keywords:");
            posY = Draw_DrawString(30, posY + SPACING_Y, COLOR_YELLOW, matched);
            Draw_DrawString(30, posY + SPACING_Y, COLOR_WHITE, "X: basho wo hyouji / show where found");
            Draw_DrawString(10, 205, COLOR_WHITE, "A: sonomama tsukau / use anyway");
            Draw_DrawString(10, 216, COLOR_WHITE, "B: tsukawanai / don't use it");
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        keys = waitInput();

        if (keys & KEY_X)
            details = !details;
    }
    while (!(keys & (KEY_A | KEY_B)) && !menuShouldExit);

    if (menuShouldExit || (keys & KEY_B))
        result = false;

    if (result && !readFailed)
        RememberApproval(hash);

    menuLeave();
    return result;
}

static Result   OpenFile(IFile *file, const char *path)
{
    return IFile_Open(file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_READ);
}

static Result   OpenPluginFile(u64 tid, IFile *plugin)
{
    if (R_FAILED(FindPluginFile(tid)) || OpenFile(plugin, g_path))
    {
        if (OpenFile(plugin, g_defaultPath))
            return -1;

        PluginLoaderCtx.pluginPath = g_defaultPath;
        PluginLoaderCtx.header.isDefaultPlugin = 1;
        return 0;
    }

    return 0;
}

static Result   CheckPluginCompatibility(_3gx_Header *header, u32 processTitle)
{
    static char   errorBuf[0x100];
    char          titleName[48];
    char          authorName[48];

    if (header->targets.count == 0)
        return 0;

    for (u32 i = 0; i < header->targets.count; ++i)
    {
        if (header->targets.titles[i] == processTitle)
            return 0;
    }

    strncpy(titleName, header->infos.titleMsg ? header->infos.titleMsg : "", sizeof(titleName) - 1);
    titleName[sizeof(titleName) - 1] = 0;
    strncpy(authorName, header->infos.authorMsg ? header->infos.authorMsg : "", sizeof(authorName) - 1);
    authorName[sizeof(authorName) - 1] = 0;

    sprintf(errorBuf, "The plugin - %s -\nis not compatible with this game.\n" \
                      "Contact \"%s\" for more infos.", titleName, authorName);

    PluginLoaderCtx.error.message = errorBuf;

    return -1;
}
bool     TryToLoadPlugin(Handle process, bool isHomebrew)
{
    u64             tid;
    u64             fileSize;
    IFile           plugin;
    Result          res;
    _3gx_Header     fileHeader;
    _3gx_Header     *header = NULL;
    _3gx_Executable *exeHdr = NULL;
    PluginLoaderContext *ctx = &PluginLoaderCtx;
    PluginHeader        *pluginHeader = &ctx->header;
    const u32           memRegionSizes[] = 
    {
        5 * 1024 * 1024, 
        2 * 1024 * 1024, 
        10 * 1024 * 1024,  
        5 * 1024 * 1024, 
    };

    
    svcGetProcessInfo((s64 *)&tid, process, 0x10001);

    memset(pluginHeader, 0, sizeof(PluginHeader));
    pluginHeader->magic = HeaderMagic;

    
    if (ctx->useUserLoadParameters && (ctx->userLoadParameters.lowTitleId == 0 || (u32)tid == ctx->userLoadParameters.lowTitleId))
    {
        if (!ctx->userLoadParameters.persistent) ctx->useUserLoadParameters = false;
        ctx->pluginMemoryStrategy = ctx->userLoadParameters.pluginMemoryStrategy;
        if (OpenFile(&plugin, ctx->userLoadParameters.path))
            return false;

        ctx->pluginPath = ctx->userLoadParameters.path;

        memcpy(pluginHeader->config, ctx->userLoadParameters.config, 32 * sizeof(u32));
    }
    else
    {
        if (R_FAILED(OpenPluginFile(tid, &plugin)))
            return false;
    }

    if (R_FAILED((res = IFile_GetSize(&plugin, &fileSize))))
        ctx->error.message = "Couldn't get file size";

    if (!res && !CheckForDestructivePlugin(&plugin, fileSize))
    {
        IFile_Close(&plugin);
        return false;
    }

    if (!res && R_FAILED(res = Check_3gx_Magic(&plugin)))
    {
        const char * errors[] = 
        {
            "Couldn't read file.",
            "Invalid plugin file\nNot a valid 3GX plugin format!",
            "Outdated plugin file\nCheck for an updated plugin.",
            "Outdated plugin loader\nCheck for Luma3DS updates."   
        };

        ctx->error.message = errors[R_MODULE(res) == RM_LDR ? R_DESCRIPTION(res) : 0];
    }

    
    if (!res && R_FAILED((res = Read_3gx_Header(&plugin, &fileHeader))))
        ctx->error.message = "Couldn't read file";

    
    if (!res && fileHeader.infos.compatibility == PLG_COMPAT_EMULATOR) {
        ctx->error.message = "Plugin is only compatible with emulators";
        res = -1;
    }

    
    if (!res && (isHomebrew && !fileHeader.infos.allowHomebrewLoad)) {
        
        ctx->error.message = NULL;
        res = -1;
    }

    
    if (!res) {
        ctx->eventsSelfManaged = fileHeader.infos.eventsSelfManaged;
        ctx->isMemPrivate = fileHeader.infos.usePrivateMemory;
        if (ctx->pluginMemoryStrategy == PLG_STRATEGY_SWAP && fileHeader.infos.swapNotNeeded)
            ctx->pluginMemoryStrategy = PLG_STRATEGY_NONE;
    }

    
    if (!res && R_FAILED((res = MemoryBlock__SetSize(memRegionSizes[fileHeader.infos.memoryRegionSize])))) {
        ctx->error.message = "Couldn't set memblock size.";
    }
    
    
    if (!res && R_FAILED((res = MemoryBlock__IsReady())))
        ctx->error.message = "Failed to allocate memory.";

    
    if (!res) {
        header = (_3gx_Header *)(ctx->memblock.memblock + g_memBlockSize - (u32)fileSize);
        memcpy(header, &fileHeader, sizeof(_3gx_Header));
    }

    
    if (!res && R_FAILED((res = Read_3gx_ParseHeader(&plugin, header))))
        ctx->error.message = "Couldn't read file";

    // Read embedded save/load functions
    if (!res && R_FAILED((res = Read_3gx_EmbeddedPayloads(&plugin, header))))
        ctx->error.message = "Invalid save/load payloads.";
    
    
    if (!res)
        ctx->exeLoadChecksum = header->infos.exeLoadChecksum;
    
    
    if (!res) res = CheckPluginCompatibility(header, (u32)tid);

    
    if (!res && R_FAILED(res = Read_3gx_LoadSegments(&plugin, header, ctx->memblock.memblock + sizeof(PluginHeader)))) {
        if (res == MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, RD_NO_DATA)) ctx->error.message = "This plugin requires a loading function.";
        else if (res == MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, RD_INVALID_ADDRESS)) ctx->error.message = "This plugin file is corrupted.";
        else ctx->error.message = "Couldn't read plugin's code";
    }

    if (R_FAILED(res))
    {
        ctx->error.code = res;
        goto exitFail;
    }

    pluginHeader->version = header->version;
    pluginHeader->waitForReplyTimeout = 10000000000ULL;
    
    exeHdr = &header->executable;
    pluginHeader->exeSize = (sizeof(PluginHeader) + exeHdr->codeSize + exeHdr->rodataSize + exeHdr->dataSize + exeHdr->bssSize + 0x1000) & ~0xFFF;
    pluginHeader->heapVA = 0x06000000;
    pluginHeader->heapSize = g_memBlockSize - pluginHeader->exeSize;
    pluginHeader->plgldrEvent = ctx->plgEventPA;
    pluginHeader->plgldrReply = ctx->plgReplyPA;

    
    PLG__NotifyEvent(PLG_OK, false);

    
    memcpy(ctx->memblock.memblock, pluginHeader, sizeof(PluginHeader));
    
    memset(ctx->memblock.memblock + pluginHeader->exeSize, 0, pluginHeader->heapSize);

    
    svcControlProcess(process, PROCESSOP_SET_MMU_TO_RWX, 0, 0);
    
    svcControlProcess(process, PROCESSOP_SIGNAL_ON_EXIT, 0, 0);

    
    if (R_SUCCEEDED(MemoryBlock__MountInProcess()))
    
    {
        u32  procStart = 0x00100000;
        u32 *game = (u32 *)procStart;

        extern u32  g_savedGameInstr[2];

        if (R_FAILED((res = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, procStart, process, procStart, 0x1000, 0))))
        {
            ctx->error.message = "Couldn't map process";
            ctx->error.code = res;
            goto exitFail;
        }

        g_savedGameInstr[0] = game[0];
        g_savedGameInstr[1] = game[1];

        game[0] = 0xE51FF004; 
        game[1] = (u32)PA_FROM_VA_PTR(gamePatchFunc);
        svcFlushEntireDataCache();
        svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, procStart, 0x1000);
    }
    else
        goto exitFail;


    IFile_Close(&plugin);
    return true;

exitFail:
    IFile_Close(&plugin);
    MemoryBlock__Free();

    return false;
}

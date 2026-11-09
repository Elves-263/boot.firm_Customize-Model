#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include "csvc.h"
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
        Draw_DrawString(10, 30, COLOR_WHITE, "A: use   X: pin/unpin   B: none");

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

static char     *memstr(const char *haystack, const char *needle, u32 size)
{
    u32     needleLen = strlen(needle);

    if (needleLen == 0 || size < needleLen)
        return NULL;

    for (const char *p = haystack; p <= haystack + size - needleLen; ++p)
    {
        if (memcmp(p, needle, needleLen) == 0)
            return (char *)p;
    }

    return NULL;
}

static const char *const g_destructiveKeywords[] =
{
    "cias", "boot.firm", "gm9", "DCIM", "JKSV", "Splashes", "boot9strap", "private", "Nintendo 3DS", "fbi"
};
#define DESTRUCTIVE_KEYWORD_COUNT (sizeof(g_destructiveKeywords) / sizeof(g_destructiveKeywords[0]))
#define DESTRUCTIVE_KEYWORD_MAXLEN 12

static bool     CheckForDestructivePlugin(IFile *plugin, u64 fileSize)
{
    static char buffer[2000];
    u64         offset = 0;
    u64         total;
    bool        found[DESTRUCTIVE_KEYWORD_COUNT] = { false };

    while (offset < fileSize)
    {
        u32 chunk = (u32)((fileSize - offset) > sizeof(buffer) ? sizeof(buffer) : (fileSize - offset));

        plugin->pos = offset;
        if (R_FAILED(IFile_Read(plugin, &total, buffer, chunk)))
            break;

        for (u32 i = 0; i < DESTRUCTIVE_KEYWORD_COUNT; ++i)
        {
            if (!found[i] && memstr(buffer, g_destructiveKeywords[i], chunk) != NULL)
                found[i] = true;
        }

        offset += (chunk > DESTRUCTIVE_KEYWORD_MAXLEN && offset + chunk < fileSize)
            ? chunk - (DESTRUCTIVE_KEYWORD_MAXLEN - 1) : chunk;
    }

    plugin->pos = 0;

    u32 hits = 0;
    for (u32 i = 0; i < DESTRUCTIVE_KEYWORD_COUNT; ++i)
        if (found[i])
            ++hits;

    u32 risk = hits * (100 / DESTRUCTIVE_KEYWORD_COUNT);

    if (risk == 0)
        return true;

    const char  *warningText = risk <= 20 ? "little dangerous" : risk <= 40 ? "moderately dangerous" : "very dangerous";
    char        textBuf[64];
    u32         keys;
    bool        result = true;

    sprintf(textBuf, "Risk : %u%% (%s)", (unsigned)risk, warningText);

    char matched[256];
    matched[0] = 0;
    for (u32 i = 0; i < DESTRUCTIVE_KEYWORD_COUNT; ++i)
    {
        if (found[i])
        {
            if (matched[0] != 0 && (u32)(strlen(matched) + 2) < (u32)sizeof(matched))
                strcat(matched, ", ");
            if ((u32)(strlen(matched) + strlen(g_destructiveKeywords[i]) + 1) < (u32)sizeof(matched))
                strcat(matched, g_destructiveKeywords[i]);
        }
    }

    menuEnter();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_RED, "--- WARNING ---");
        u32 posY = Draw_DrawString(30, 30, COLOR_WHITE, "This 3gx may be a destructive plugin!");
        posY = Draw_DrawString(30, posY + SPACING_Y, COLOR_WHITE, textBuf);
        posY = Draw_DrawString(30, posY + SPACING_Y, COLOR_WHITE, "Matched keywords:");
        posY = Draw_DrawString(30, posY + SPACING_Y, COLOR_YELLOW, matched);
        Draw_DrawString(30, posY + SPACING_Y, COLOR_WHITE, "A: use it anyway   B: don't use it");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        keys = waitInput();
    }
    while (!(keys & (KEY_A | KEY_B)) && !menuShouldExit);

    if (menuShouldExit || (keys & KEY_B))
        result = false;

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

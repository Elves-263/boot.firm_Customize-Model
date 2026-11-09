#pragma once

#include <stddef.h>
#include "bearssl.h"

// trust_anchors.c（ルート証明書の一覧から作る）
extern const br_x509_trust_anchor g_trustAnchors[];
extern const size_t g_trustAnchorCount;

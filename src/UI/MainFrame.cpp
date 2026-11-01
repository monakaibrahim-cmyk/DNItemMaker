#include "UI/MainFrame.h"
#include "UI/SplashWnd.h"
#include "UI/ModalDialogs.h"

#include "Common/Crash/CrashHandler.h"
#include "Common/Logs/Logs.h"
#include "version.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <uxtheme.h>

namespace DNItemMaker::UI {

namespace {

constexpr UINT kMsgWorkspaceLoadFinished = WM_APP + 10;
constexpr UINT kMsgModelIndexReady = WM_APP + 11;
constexpr UINT_PTR kSearchDebounceTimer = 2001;

enum ControlIds : UINT {
    ID_CMD_OPEN_WORKSPACE = 40001,
    ID_CMD_RELOAD_WORKSPACE,
    ID_CMD_SAVE_TO_FOLDER,
    ID_CMD_COMMIT_DISK,
    ID_CMD_DISCARD_DRAFTS,
    ID_CMD_EXIT_APP,

    ID_CMD_ITEM_NEW,
    ID_CMD_ITEM_DUPLICATE,
    ID_CMD_ITEM_APPLY,
    ID_CMD_ITEM_REVERT,
    ID_CMD_ITEM_DELETE,
    ID_CMD_ITEM_UNDO,
    ID_CMD_ITEM_PICK_ICON,
    ID_CMD_FOCUS_SEARCH,

    ID_CMD_VERIFY_ROUNDTRIP,
    ID_CMD_WORKSPACE_AUDIT,
    ID_CMD_CLEAR_CACHE,
    ID_CMD_CAPTURE_SNAPSHOT,
    ID_CMD_OPEN_LOGS,
    ID_CMD_HELP_ABOUT,

    // Context Menu Commands
    ID_CTX_FORK_MIDS = 40101,
    ID_CTX_GEN_TIER_SET,
    ID_CTX_GEN_CLASS_SERIES,
    ID_CTX_USE_AS_DONOR,
    ID_CTX_COPY_ID,
    ID_CTX_COPY_JSON,

    // Tab Shortcut Commands (Ctrl+1 .. Ctrl+7)
    ID_CMD_TAB_1 = 40201,
    ID_CMD_TAB_2,
    ID_CMD_TAB_3,
    ID_CMD_TAB_4,
    ID_CMD_TAB_5,
    ID_CMD_TAB_6,
    ID_CMD_TAB_7,
    ID_CMD_TAB_8,
    ID_CMD_TOOLS_CHARM_CALC,
    ID_CMD_TOOLS_DRAGON_JEWEL,
    ID_CMD_TOOLS_SET_ITEM,
    ID_CMD_TOOLS_ENCHANT,
    ID_CTX_OPEN_DRAGON_JEWEL,
    ID_CTX_OPEN_SET_ITEM,
    ID_CTX_OPEN_ENCHANT,
    IDC_BTN_PROP_DRAGON_JEWEL,
    IDC_BTN_PROP_SET_ITEM,
    IDC_BTN_PROP_ENCHANT,
    ID_CMD_CHARM_SYNC_ITEM,
    ID_CMD_CHARM_REFRESH,
    ID_CMD_CHARM_EXPORT_CSV,
    ID_CMD_CHARM_NORMALIZE,
    ID_CMD_CHARM_APPLY_WEIGHT,
    ID_CMD_SIM_PULL_1,
    ID_CMD_SIM_PULL_10,
    ID_CMD_SIM_PULL_100,
    ID_CMD_SIM_PULL_1000,
    ID_CMD_SIM_JACKPOT,
    ID_CMD_SIM_RESET,
    IDC_SEARCH_EDIT = 41001,
    IDC_SEARCH_CLEAR_BTN,
    IDC_TABLE_FILTER_COMBO,
    IDC_GROUP_FILTER_COMBO,
    IDC_RANK_FILTER_COMBO,
    IDC_CHK_MODIFIED_ONLY,
    IDC_ITEM_LIST,
    IDC_SUMMARY_BANNER = 42000,
    IDC_TAB_CTRL = 42001,
    IDC_BANNER_ICON = 42002,
    IDC_EDIT_RANK_COMBO = 42010,
    IDC_EDIT_ICON_INDEX,
    IDC_EDIT_LEVEL,
    IDC_EDIT_HERO_LEVEL,
    IDC_EDIT_OVERLAP,
    IDC_EDIT_SELL,
    IDC_BTN_AUTO_SELL,
    IDC_EDIT_GEAR_SCORE,
    IDC_BTN_AUTO_GS,
    IDC_EDIT_DURABILITY,
    IDC_BTN_AUTO_DURA,
    IDC_BTN_AUTO_ALL_ECON,
    IDC_CHK_CASH,
    IDC_CHK_STORAGE,
    IDC_CHK_WSTORAGE,
    IDC_BTN_PRESET_EQUIP,
    IDC_BTN_PRESET_STACK,
    IDC_BTN_PRESET_CASH,
    IDC_EDIT_JOBS,
    IDC_CHK_CLASS_ALL = 42040,
    IDC_CHK_CLASS_BASE_FIRST = 42041,
    IDC_CHK_CLASS_LAST = 42050,
    IDC_TREE_SUBCLASSES = 42051,
    IDC_BTN_SUBCLASS_ALL_1ST = 42052,
    IDC_BTN_SUBCLASS_ALL_2ND = 42053,
    IDC_BTN_SUBCLASS_CLEAR = 42054,
    IDC_BTN_SUBCLASS_EXPAND = 42055,
    ID_CMD_SUBCLASS_CHECK_BRANCH = 42056,
    ID_CMD_SUBCLASS_UNCHECK_BRANCH = 42057,
    ID_CMD_SUBCLASS_TOGGLE_EXPAND = 42058,
    IDC_BTN_FORK_MIDS = 42060,
    IDC_BTN_GEN_TOOLTIP,
    IDC_EDIT_NAME_MID,
    IDC_BTN_ALLOC_NAME_MID,
    IDC_BTN_SEARCH_NAME_MID,
    IDC_EDIT_NAME_PARAM,
    IDC_EDIT_NAME_TMPL,
    IDC_EDIT_NAME_RESOLVED,
    IDC_EDIT_DESC_MID,
    IDC_BTN_ALLOC_DESC_MID,
    IDC_BTN_SEARCH_DESC_MID,
    IDC_EDIT_DESC_PARAM,
    IDC_EDIT_DESC_TMPL,
    IDC_EDIT_DESC_RESOLVED,
    IDC_BTN_COLOR_FIRST = 42080,
    IDC_BTN_COLOR_LAST = 42086,
    IDC_STAT_BASE = 42100, // 42100..42139
    IDC_CMB_ARCHETYPE = 42150,
    IDC_BTN_APPLY_ARCHETYPE,
    IDC_BTN_STATS_AUTOCURVE,
    IDC_BTN_STAT_SCALE_FIRST = 42155,
    IDC_BTN_STAT_SCALE_LAST = 42159,
    IDC_BTN_STATS_SPREAD15 = 42160,
    IDC_BTN_STATS_MIN_TO_MAX,
    IDC_BTN_STATS_PROBS_FULL,
    IDC_BTN_STATS_COMPACT,
    IDC_BTN_STATS_COPY,
    IDC_BTN_STATS_PASTE,
    IDC_BTN_STATS_CLEAR,
    IDC_PREVIEW_WND = 42200,
    IDC_CMB_RENDER_MODE,
    IDC_CHK_AUTOSPIN,
    IDC_BTN_ZOOM_IN_3D,
    IDC_BTN_ZOOM_OUT_3D,
    IDC_BTN_RESET_CAMERA,
    IDC_CMB_CHAR_CLASS = 42215,
    IDC_CMB_CHAR_ACTION,
    IDC_BTN_ANIM_PLAY_PAUSE,
    IDC_BTN_ANIM_RESET,
    IDC_CHK_ANIM_LOOP,
    IDC_CMB_ANIM_SPEED,
    IDC_BTN_PROP_ICON_EDITOR,
    IDC_BTN_TOOLS_ACT_CONVERT,
    IDC_BTN_TOOLS_ICON_STUDIO,
    IDC_EDIT_DONOR_ID,
    IDC_BTN_USE_SEL_DONOR,
    IDC_BTN_CLONE_VISUAL_BOTH,
    IDC_BTN_CLONE_SKIN_ONLY,
    IDC_BTN_CLONE_ICON_ONLY,
    IDC_RAW_SOURCE_COMBO = 42230,
    IDC_RAW_FILTER_EDIT,
    IDC_RAW_COPY_TSV_BTN,
    IDC_RAW_CLONE_DETAIL_BTN,
    IDC_RAW_COL_LIST,
    IDC_RAW_VALUE_EDIT,
    IDC_RAW_WRITE_BTN,
    IDC_TIER_BASENAME_EDIT = 42300,
    IDC_TIER_GROWTH_EDIT,
    IDC_TIER_RANK_FIRST = 42305, // 42305..42311
    IDC_TIER_AUTOCURVE_CHK = 42315,
    IDC_TIER_AUTOECON_CHK,
    IDC_TIER_RICHDESC_CHK,
    IDC_BTN_RUN_TIER_SET,

    IDC_SERIES_TMPL_EDIT = 42330,
    IDC_SERIES_CLASS_FIRST = 42335, // 42335..42344
    IDC_SERIES_ADAPT_CHK = 42346,
    IDC_SERIES_RICHDESC_CHK,
    IDC_BTN_RUN_CLASS_SERIES,

    IDC_BATCH_SCALE_CHK = 42360,
    IDC_BATCH_SCALE_EDIT,
    IDC_BATCH_AUTOCURVE_CHK,
    IDC_BATCH_SANITIZE_CHK,
    IDC_BATCH_AUTOECON_CHK,
    IDC_BATCH_SETRANK_CHK,
    IDC_BATCH_RANK_CMB,
    IDC_BATCH_SETLEVEL_CHK,
    IDC_BATCH_LEVEL_EDIT,
    IDC_BATCH_SETOVERLAP_CHK,
    IDC_BATCH_OVERLAP_EDIT,
    IDC_BATCH_PREFIX_EDIT,
    IDC_BATCH_SUFFIX_EDIT,
    IDC_BTN_RUN_BATCH_EDIT,
    IDC_BTN_JSON_EXP_SEL = 42400,
    IDC_BTN_JSON_EXP_MOD,
    IDC_BTN_JSON_EXP_FILT,
    IDC_BTN_JSON_IMP_FILE,
    IDC_BTN_JSON_IMP_CLIP,
    IDC_MODIFIED_ITEMS_LIST,
    IDC_BTN_MOD_REFRESH,
    IDC_CHK_AUTO_APPLY = 42450,
    IDC_CHARM_SELECTOR = 42500,
    IDC_BTN_CHARM_SYNC,
    IDC_BTN_CHARM_REFRESH,
    IDC_BTN_CHARM_EXPORT_CSV,
    IDC_LST_CHARM_REWARDS,
    IDC_BTN_SIM_PULL_1,
    IDC_BTN_SIM_PULL_10,
    IDC_BTN_SIM_PULL_100,
    IDC_BTN_SIM_PULL_1000,
    IDC_BTN_SIM_JACKPOT,
    IDC_BTN_SIM_RESET,
    IDC_EDIT_SIM_LOG,
    IDC_EDIT_CHARM_TARGET_WEIGHT,
    IDC_BTN_CHARM_NORMALIZE,
    IDC_EDIT_CHARM_EDIT_WEIGHT,
    IDC_BTN_CHARM_APPLY_WEIGHT,
    IDC_BTN_LAUNCH_CHARM_CALC,
};

constexpr const wchar_t* kBaseClassLabels[10] = {
    L"1 Warrior", L"2 Archer", L"3 Sorceress", L"4 Cleric", L"5 Academic",
    L"6 Kali",    L"7 Assassin", L"8 Lancea",  L"9 Machina", L"201 Vandar"
};
constexpr int32_t kBaseClassIds[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 201 };

struct ClassNodeInfo {
    int32_t jobId;
    int32_t parentId; // 0 for base classes, or parent's jobId
    int32_t baseId;
    int tier;         // 0 = Base, 1 = 1st Spec, 2 = 2nd Spec
    const wchar_t* name;
};

constexpr ClassNodeInfo kDNClassHierarchy[] = {
    // === Warrior (1) ===
    { 1, 0, 1, 0, L"Warrior" },
    { 11, 1, 1, 1, L"Swordmaster" },
    { 23, 11, 1, 2, L"Gladiator" },
    { 24, 11, 1, 2, L"Moonlord" },
    { 12, 1, 1, 1, L"Mercenary" },
    { 25, 12, 1, 2, L"Barbarian" },
    { 26, 12, 1, 2, L"Destroyer" },
    { 75, 1, 1, 1, L"Avenger" },
    { 76, 75, 1, 2, L"Dark Avenger" },
    { 106, 1, 1, 1, L"Knight" },
    { 107, 106, 1, 2, L"Mystic Knight" },
    { 108, 106, 1, 2, L"Grand Master" },

    // === Archer (2) ===
    { 2, 0, 2, 0, L"Archer" },
    { 14, 2, 2, 1, L"Bowmaster" },
    { 29, 14, 2, 2, L"Sniper" },
    { 30, 14, 2, 2, L"Artillery" },
    { 15, 2, 2, 1, L"Acrobat" },
    { 31, 15, 2, 2, L"Tempest" },
    { 32, 15, 2, 2, L"Wind Walker" },
    { 80, 2, 2, 1, L"Hunter" },
    { 81, 80, 2, 2, L"Silver Hunter" },

    // === Sorceress (3) ===
    { 3, 0, 3, 0, L"Sorceress" },
    { 17, 3, 3, 1, L"Elemental Lord" },
    { 35, 17, 3, 2, L"Saleana" },
    { 36, 17, 3, 2, L"Elestra" },
    { 18, 3, 3, 1, L"Force User" },
    { 37, 18, 3, 2, L"Smasher" },
    { 38, 18, 3, 2, L"Majesty" },
    { 84, 3, 3, 1, L"Mara" },
    { 85, 84, 3, 2, L"Black Mara" },

    // === Cleric (4) ===
    { 4, 0, 4, 0, L"Cleric" },
    { 20, 4, 4, 1, L"Paladin" },
    { 41, 20, 4, 2, L"Guardian" },
    { 42, 20, 4, 2, L"Crusader" },
    { 22, 4, 4, 1, L"Priest" },
    { 43, 22, 4, 2, L"Saint" },
    { 44, 22, 4, 2, L"Inquisitor" },
    { 82, 4, 4, 1, L"Heretic" },
    { 83, 82, 4, 2, L"Arch Heretic" },

    // === Academic (5) ===
    { 5, 0, 5, 0, L"Academic" },
    { 46, 5, 5, 1, L"Engineer" },
    { 47, 46, 5, 2, L"Shooting Star" },
    { 48, 46, 5, 2, L"Gear Master" },
    { 49, 5, 5, 1, L"Alchemist" },
    { 50, 49, 5, 2, L"Adept" },
    { 51, 49, 5, 2, L"Physician" },
    { 86, 5, 5, 1, L"Mechanic" },
    { 87, 86, 5, 2, L"Ray Mechanic" },

    // === Kali (6) ===
    { 6, 0, 6, 0, L"Kali" },
    { 54, 6, 6, 1, L"Screamer" },
    { 55, 54, 6, 2, L"Dark Summoner" },
    { 56, 54, 6, 2, L"Soul Eater" },
    { 57, 6, 6, 1, L"Dancer" },
    { 58, 57, 6, 2, L"Blade Dancer" },
    { 59, 57, 6, 2, L"Spirit Dancer" },
    { 88, 6, 6, 1, L"Oracle" },
    { 89, 88, 6, 2, L"Oracle Elder" },

    // === Assassin (7) ===
    { 7, 0, 7, 0, L"Assassin" },
    { 62, 7, 7, 1, L"Chaser" },
    { 63, 62, 7, 2, L"Ripper" },
    { 64, 62, 7, 2, L"Raven" },
    { 67, 7, 7, 1, L"Bringer" },
    { 68, 67, 7, 2, L"Light Fury" },
    { 69, 67, 7, 2, L"Abyss Walker" },
    { 90, 7, 7, 1, L"Phantom" },
    { 91, 90, 7, 2, L"Bleed Phantom" },

    // === Lancea (8) ===
    { 8, 0, 8, 0, L"Lancea" },
    { 72, 8, 8, 1, L"Piercer" },
    { 73, 72, 8, 2, L"Flurry" },
    { 74, 72, 8, 2, L"Sting Breezer" },
    { 92, 8, 8, 1, L"Knightess" },
    { 93, 92, 8, 2, L"Avalanche" },
    { 94, 92, 8, 2, L"Randgrid" },
    { 98, 8, 8, 1, L"Plaga" },
    { 99, 98, 8, 2, L"Vena Plaga" },

    // === Machina (9) ===
    { 9, 0, 9, 0, L"Machina" },
    { 77, 9, 9, 1, L"Patrona" },
    { 78, 77, 9, 2, L"Defensio" },
    { 79, 77, 9, 2, L"Ruina" },
    { 95, 9, 9, 1, L"Launcher" },
    { 96, 95, 9, 2, L"Impactor" },
    { 97, 95, 9, 2, L"Luster" },
    { 109, 9, 9, 1, L"Beastia" },
    { 110, 109, 9, 2, L"Beastia Reina" },

    // === Vandar (201) ===
    { 201, 0, 201, 0, L"Vandar" },
    { 100, 201, 201, 1, L"Treasure Hunter" },
    { 102, 100, 201, 2, L"Duelist" },
    { 103, 100, 201, 2, L"Trickster" },
    { 101, 201, 201, 1, L"Wanderer" },
    { 104, 101, 201, 2, L"Revenant" },
    { 105, 101, 201, 2, L"Maverick" },
};

bool PickFolderDialog(HWND owner, const std::filesystem::path& initialDir, std::filesystem::path& outFolder) {
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = ::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return false;

    DWORD opts = 0;
    if (SUCCEEDED(dlg->GetOptions(&opts))) {
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    }
    dlg->SetTitle(L"Select Dragon Nest 'resource' Directory (containing ext/ and uistring/)");

    if (!initialDir.empty()) {
        IShellItem* initItem = nullptr;
        const std::wstring wInit = initialDir.wstring();
        if (SUCCEEDED(::SHCreateItemFromParsingName(wInit.c_str(), nullptr, IID_PPV_ARGS(&initItem))) && initItem) {
            dlg->SetFolder(initItem);
            initItem->Release();
        }
    }

    bool picked = false;
    if (SUCCEEDED(dlg->Show(owner))) {
        IShellItem* result = nullptr;
        if (SUCCEEDED(dlg->GetResult(&result)) && result) {
            PWSTR pszPath = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath) {
                outFolder = std::filesystem::path(pszPath);
                picked = true;
                ::CoTaskMemFree(pszPath);
            }
            result->Release();
        }
    }
    dlg->Release();
    return picked;
}

bool CopyUtf8ToClipboard(HWND owner, std::string_view utf8) {
    if (!::OpenClipboard(owner)) return false;
    ::EmptyClipboard();
    const CString wide = Utf8ToWide(utf8);
    const size_t bytes = static_cast<size_t>(wide.GetLength() + 1) * sizeof(wchar_t);
    bool ok = false;
    if (HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (void* ptr = ::GlobalLock(hMem)) {
            std::memcpy(ptr, wide.GetString(), bytes);
            ::GlobalUnlock(hMem);
            ok = (::SetClipboardData(CF_UNICODETEXT, hMem) != nullptr);
        }
    }
    ::CloseClipboard();
    return ok;
}

std::string ReadUtf8FromClipboard(HWND owner) {
    std::string out;
    if (!::OpenClipboard(owner)) return out;
    if (HANDLE hData = ::GetClipboardData(CF_UNICODETEXT)) {
        if (const auto* wPtr = static_cast<const wchar_t*>(::GlobalLock(hData))) {
            out = WideToUtf8(CString(wPtr));
            ::GlobalUnlock(hData);
        }
    }
    ::CloseClipboard();
    return out;
}

constexpr char AsciiLower(char c) noexcept {
    const auto u = static_cast<unsigned char>(c);
    return static_cast<char>((u >= 'A' && u <= 'Z') ? (u + ('a' - 'A')) : u);
}

bool ContainsAsciiCaseInsensitive(std::string_view haystack, std::string_view lowerNeedle) noexcept {
    if (lowerNeedle.empty()) return true;
    if (haystack.size() < lowerNeedle.size()) return false;
    const size_t limit = haystack.size() - lowerNeedle.size();
    for (size_t i = 0; i <= limit; ++i) {
        size_t j = 0;
        while (j < lowerNeedle.size() && AsciiLower(haystack[i + j]) == lowerNeedle[j]) {
            ++j;
        }
        if (j == lowerNeedle.size()) return true;
    }
    return false;
}

std::string GetEditUtf8(const CEdit& edt) {
    CString txt;
    edt.GetWindowTextW(txt);
    return WideToUtf8(txt);
}

std::string_view TrimAscii(std::string_view sv) noexcept {
    while (!sv.empty() && static_cast<unsigned char>(sv.front()) <= ' ') sv.remove_prefix(1);
    while (!sv.empty() && static_cast<unsigned char>(sv.back()) <= ' ') sv.remove_suffix(1);
    return sv;
}

int32_t ParseIntOr(const CEdit& edt, int32_t fallback = 0) {
    const std::string utf8 = GetEditUtf8(edt);
    std::string_view sv = TrimAscii(utf8);
    if (!sv.empty() && sv.front() == '#') sv.remove_prefix(1);
    if (sv.empty()) return fallback;
    int32_t v = fallback;
    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return (ec == std::errc{} && ptr == sv.data() + sv.size()) ? v : fallback;
}

uint32_t ParseUIntOr(const CEdit& edt, uint32_t fallback = 0) {
    const std::string utf8 = GetEditUtf8(edt);
    const std::string_view sv = TrimAscii(utf8);
    if (sv.empty()) return fallback;
    uint32_t v = fallback;
    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return (ec == std::errc{} && ptr == sv.data() + sv.size()) ? v : fallback;
}

float ParseFloatOr(const CEdit& edt, float fallback = 0.0f) {
    const std::string utf8 = GetEditUtf8(edt);
    const std::string_view sv = TrimAscii(utf8);
    if (sv.empty()) return fallback;
    float v = fallback;
    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return (ec == std::errc{} && ptr == sv.data() + sv.size()) ? v : fallback;
}

double ParseDoubleOr(const CEdit& edt, double fallback = 1.0) {
    const std::string utf8 = GetEditUtf8(edt);
    const std::string_view sv = TrimAscii(utf8);
    if (sv.empty()) return fallback;
    double v = fallback;
    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return (ec == std::errc{} && ptr == sv.data() + sv.size()) ? v : fallback;
}

bool ItemsEqualForAutoApply(const Data::DNItem& a, const Data::DNItem& b) noexcept {
    if (a.rank != b.rank || a.iconIndex != b.iconIndex || a.levelLimit != b.levelLimit ||
        a.heroLevelLimit != b.heroLevelLimit || a.overlapCount != b.overlapCount ||
        a.sellAmount != b.sellAmount || a.gearScore != b.gearScore || a.durability != b.durability ||
        a.isCash != b.isCash || a.ableStorage != b.ableStorage || a.ableWStorage != b.ableWStorage ||
        a.nameId != b.nameId || a.descId != b.descId || a.jobClass != b.jobClass ||
        a.nameParam != b.nameParam || a.nameTemplate != b.nameTemplate ||
        a.descParam != b.descParam || a.descTemplate != b.descTemplate) {
        return false;
    }
    for (size_t i = 0; i < Data::kStateSlotCount; ++i) {
        if (a.states[i].type != b.states[i].type ||
            std::fabs(a.states[i].genProb - b.states[i].genProb) > 1e-4f ||
            a.states[i].minValue != b.states[i].minValue ||
            a.states[i].maxValue != b.states[i].maxValue) {
            return false;
        }
    }
    return true;
}

} // namespace

BEGIN_MESSAGE_MAP(CIconSlotWnd, CWnd)
    ON_WM_ERASEBKGND()
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

bool CIconSlotWnd::Create(CWnd* parent, const CRect& rect, UINT id, UINT clickCmdId) {
    m_clickCmdId = clickCmdId;
    LPCTSTR wndCls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
                                        ::LoadCursor(nullptr, IDC_HAND),
                                        Theme::ChromeBrush());
    return CWnd::Create(wndCls, L"", WS_CHILD | WS_VISIBLE, rect, parent, id) != FALSE;
}

void CIconSlotWnd::SetIcon(const Graphics::ImageBuffer* icon) {
    if (icon && icon->width > 0 && icon->height > 0) {
        m_icon = *icon;
        m_hasIcon = true;
    } else {
        m_hasIcon = false;
        m_icon = {};
    }
    if (GetSafeHwnd()) Invalidate(FALSE);
}

BOOL CIconSlotWnd::OnEraseBkgnd(CDC*) {
    return TRUE;
}

void CIconSlotWnd::OnPaint() {
    CPaintDC paintDC(this);
    CRect rc;
    GetClientRect(&rc);
    if (rc.Width() <= 0 || rc.Height() <= 0) return;

    Theme::FrameRect1px(paintDC, rc, Theme::kBorder);

    CRect inner = rc;
    inner.DeflateRect(1, 1);
    paintDC.FillSolidRect(&inner, RGB(20, 22, 28));

    if (m_hasIcon) {
        BlitImageBuffer(paintDC.GetSafeHdc(), inner, m_icon, RGB(20, 22, 28), false);
    }
}

void CIconSlotWnd::OnLButtonDown(UINT nFlags, CPoint pt) {
    CWnd::OnLButtonDown(nFlags, pt);
    if (m_clickCmdId && GetParent()) {
        GetParent()->PostMessageW(WM_COMMAND, m_clickCmdId, 0);
    }
}

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_ERASEBKGND()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_WM_CTLCOLOR()

    ON_MESSAGE(kMsgWorkspaceLoadFinished, &CMainFrame::OnWorkspaceLoadFinished)
    ON_MESSAGE(kMsgModelIndexReady, &CMainFrame::OnModelIndexReady)

    // Menu, Top Command Bar & Accelerator commands
    ON_COMMAND(ID_CMD_OPEN_WORKSPACE, &CMainFrame::OnFileOpenWorkspace)
    ON_COMMAND(ID_CMD_RELOAD_WORKSPACE, &CMainFrame::OnFileReloadWorkspace)
    ON_COMMAND(ID_CMD_SAVE_TO_FOLDER, &CMainFrame::OnFileSaveToFolder)
    ON_COMMAND(ID_CMD_COMMIT_DISK, &CMainFrame::OnFileCommitToDisk)
    ON_COMMAND(ID_CMD_DISCARD_DRAFTS, &CMainFrame::OnFileDiscardDrafts)
    ON_COMMAND(ID_CMD_EXIT_APP, &CMainFrame::OnAppExit)

    ON_COMMAND(ID_CMD_ITEM_NEW, &CMainFrame::OnItemNew)
    ON_COMMAND(ID_CMD_ITEM_DUPLICATE, &CMainFrame::OnItemDuplicate)
    ON_COMMAND(ID_CMD_ITEM_APPLY, &CMainFrame::OnItemApply)
    ON_COMMAND(ID_CMD_ITEM_REVERT, &CMainFrame::OnItemRevert)
    ON_COMMAND(ID_CMD_ITEM_DELETE, &CMainFrame::OnItemDelete)
    ON_COMMAND(ID_CMD_ITEM_UNDO, &CMainFrame::OnItemUndo)
    ON_COMMAND(ID_CMD_ITEM_PICK_ICON, &CMainFrame::OnItemPickIcon)
    ON_BN_CLICKED(IDC_BTN_PROP_ICON_EDITOR, &CMainFrame::OnOpenIconEditor)
    ON_BN_CLICKED(IDC_BTN_TOOLS_ACT_CONVERT, &CMainFrame::OnOpenActConverter)
    ON_BN_CLICKED(IDC_BTN_TOOLS_ICON_STUDIO, &CMainFrame::OnOpenIconEditor)
    ON_COMMAND(IDC_BTN_PROP_ICON_EDITOR, &CMainFrame::OnOpenIconEditor)
    ON_COMMAND(IDC_BTN_TOOLS_ACT_CONVERT, &CMainFrame::OnOpenActConverter)
    ON_COMMAND(IDC_BTN_TOOLS_ICON_STUDIO, &CMainFrame::OnOpenIconEditor)
    ON_COMMAND(kCmdPickIconFromPreview, &CMainFrame::OnItemPickIcon)
    ON_COMMAND(ID_CMD_FOCUS_SEARCH, &CMainFrame::OnFocusSearch)

    ON_COMMAND(ID_CMD_VERIFY_ROUNDTRIP, &CMainFrame::OnToolsVerifyRoundTrip)
    ON_COMMAND(ID_CMD_WORKSPACE_AUDIT, &CMainFrame::OnToolsWorkspaceAudit)
    ON_COMMAND(ID_CMD_CLEAR_CACHE, &CMainFrame::OnToolsClearCache)
    ON_COMMAND(ID_CMD_CAPTURE_SNAPSHOT, &CMainFrame::OnToolsCaptureSnapshot)
    ON_COMMAND(ID_CMD_OPEN_LOGS, &CMainFrame::OnToolsOpenLogsFolder)
    ON_COMMAND(ID_CMD_HELP_ABOUT, &CMainFrame::OnHelpAbout)
    ON_COMMAND(ID_CMD_TOOLS_DRAGON_JEWEL, &CMainFrame::OnToolsDragonJewelStudio)
    ON_COMMAND(ID_CMD_TOOLS_SET_ITEM, &CMainFrame::OnToolsSetItemStudio)
    ON_COMMAND(ID_CMD_TOOLS_ENCHANT, &CMainFrame::OnToolsEnchantStudio)
    ON_COMMAND(IDC_BTN_PROP_DRAGON_JEWEL, &CMainFrame::OnToolsDragonJewelStudio)
    ON_COMMAND(IDC_BTN_PROP_SET_ITEM, &CMainFrame::OnToolsSetItemStudio)
    ON_COMMAND(IDC_BTN_PROP_ENCHANT, &CMainFrame::OnToolsEnchantStudio)
    ON_COMMAND(ID_CTX_OPEN_DRAGON_JEWEL, &CMainFrame::OnToolsDragonJewelStudio)
    ON_COMMAND(ID_CTX_OPEN_SET_ITEM, &CMainFrame::OnToolsSetItemStudio)
    ON_COMMAND(ID_CTX_OPEN_ENCHANT, &CMainFrame::OnToolsEnchantStudio)
    ON_EN_CHANGE(IDC_SEARCH_EDIT, &CMainFrame::OnSearchTextChanged)
    ON_BN_CLICKED(IDC_SEARCH_CLEAR_BTN, &CMainFrame::OnClearSearch)
    ON_CBN_SELCHANGE(IDC_TABLE_FILTER_COMBO, &CMainFrame::OnFilterChanged)
    ON_CBN_SELCHANGE(IDC_GROUP_FILTER_COMBO, &CMainFrame::OnFilterChanged)
    ON_CBN_SELCHANGE(IDC_RANK_FILTER_COMBO, &CMainFrame::OnFilterChanged)
    ON_BN_CLICKED(IDC_CHK_MODIFIED_ONLY, &CMainFrame::OnFilterChanged)
    ON_NOTIFY(LVN_GETDISPINFO, IDC_ITEM_LIST, &CMainFrame::OnItemListGetDispInfo)
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_ITEM_LIST, &CMainFrame::OnItemListCustomDraw)
    ON_NOTIFY(NM_SETFOCUS, IDC_ITEM_LIST, &CMainFrame::OnItemListFocusChange)
    ON_NOTIFY(NM_KILLFOCUS, IDC_ITEM_LIST, &CMainFrame::OnItemListFocusChange)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_ITEM_LIST, &CMainFrame::OnItemListSelectionChanged)
    ON_NOTIFY(LVN_COLUMNCLICK, IDC_ITEM_LIST, &CMainFrame::OnItemListColumnClick)
    ON_NOTIFY(NM_RCLICK, IDC_ITEM_LIST, &CMainFrame::OnItemListRightClick)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_CTRL, &CMainFrame::OnTabSelChange)
    ON_COMMAND_RANGE(ID_CMD_TAB_1, ID_CMD_TAB_8, &CMainFrame::OnTabShortcut)
    ON_COMMAND(ID_CMD_TOOLS_CHARM_CALC, &CMainFrame::OnToolsCharmCalculator)
    ON_COMMAND(IDC_BTN_LAUNCH_CHARM_CALC, &CMainFrame::OnToolsCharmCalculator)
    ON_COMMAND(ID_CMD_CHARM_SYNC_ITEM, &CMainFrame::OnCharmSyncCurrentItem)
    ON_COMMAND(ID_CMD_CHARM_REFRESH, &CMainFrame::OnCharmRefresh)
    ON_COMMAND(ID_CMD_CHARM_EXPORT_CSV, &CMainFrame::OnCharmExportCsv)
    ON_COMMAND(ID_CMD_CHARM_NORMALIZE, &CMainFrame::OnCharmNormalizeWeights)
    ON_COMMAND(ID_CMD_CHARM_APPLY_WEIGHT, &CMainFrame::OnCharmApplySelectedWeight)
    ON_COMMAND(ID_CMD_SIM_PULL_1, &CMainFrame::OnSimPull1)
    ON_COMMAND(ID_CMD_SIM_PULL_10, &CMainFrame::OnSimPull10)
    ON_COMMAND(ID_CMD_SIM_PULL_100, &CMainFrame::OnSimPull100)
    ON_COMMAND(ID_CMD_SIM_PULL_1000, &CMainFrame::OnSimPull1000)
    ON_COMMAND(ID_CMD_SIM_JACKPOT, &CMainFrame::OnSimJackpot)
    ON_COMMAND(ID_CMD_SIM_RESET, &CMainFrame::OnSimReset)
    ON_CBN_SELCHANGE(IDC_CHARM_SELECTOR, &CMainFrame::OnCharmSelectorChange)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LST_CHARM_REWARDS, &CMainFrame::OnCharmRewardItemSelect)
    ON_CBN_SELCHANGE(IDC_EDIT_RANK_COMBO, &CMainFrame::OnRankComboChanged)
    ON_EN_CHANGE(IDC_EDIT_ICON_INDEX, &CMainFrame::OnIconIndexEditChanged)
    ON_EN_CHANGE(IDC_EDIT_JOBS, &CMainFrame::OnJobsEditChanged)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHK_CLASS_ALL, IDC_CHK_CLASS_LAST, &CMainFrame::OnClassMatrixClicked)
    ON_NOTIFY(NM_CLICK, IDC_TREE_SUBCLASSES, &CMainFrame::OnSubclassTreeClick)
    ON_NOTIFY(TVN_KEYDOWN, IDC_TREE_SUBCLASSES, &CMainFrame::OnSubclassTreeKeyDown)
    ON_NOTIFY(NM_RCLICK, IDC_TREE_SUBCLASSES, &CMainFrame::OnSubclassTreeRClick)
    ON_BN_CLICKED(IDC_BTN_SUBCLASS_ALL_1ST, &CMainFrame::OnSubclassAll1st)
    ON_BN_CLICKED(IDC_BTN_SUBCLASS_ALL_2ND, &CMainFrame::OnSubclassAll2nd)
    ON_BN_CLICKED(IDC_BTN_SUBCLASS_CLEAR, &CMainFrame::OnSubclassClear)
    ON_BN_CLICKED(IDC_BTN_SUBCLASS_EXPAND, &CMainFrame::OnSubclassToggleExpand)
    ON_COMMAND(ID_CMD_SUBCLASS_CHECK_BRANCH, &CMainFrame::OnSubclassCheckBranch)
    ON_COMMAND(ID_CMD_SUBCLASS_UNCHECK_BRANCH, &CMainFrame::OnSubclassUncheckBranch)
    ON_COMMAND(ID_CMD_SUBCLASS_TOGGLE_EXPAND, &CMainFrame::OnSubclassToggleExpand)
    ON_BN_CLICKED(IDC_BTN_AUTO_SELL, &CMainFrame::OnAutoCalcSell)
    ON_BN_CLICKED(IDC_BTN_AUTO_GS, &CMainFrame::OnAutoCalcGearScore)
    ON_BN_CLICKED(IDC_BTN_AUTO_DURA, &CMainFrame::OnAutoCalcDurability)
    ON_BN_CLICKED(IDC_BTN_AUTO_ALL_ECON, &CMainFrame::OnAutoCalcAllEconomy)
    ON_BN_CLICKED(IDC_BTN_PRESET_EQUIP, &CMainFrame::OnPresetEquipDefaults)
    ON_BN_CLICKED(IDC_BTN_PRESET_STACK, &CMainFrame::OnPresetStack999)
    ON_BN_CLICKED(IDC_BTN_PRESET_CASH, &CMainFrame::OnPresetCashBound)
    ON_EN_CHANGE(IDC_EDIT_NAME_MID, &CMainFrame::OnLiveTextPreviewChanged)
    ON_EN_CHANGE(IDC_EDIT_DESC_MID, &CMainFrame::OnLiveTextPreviewChanged)
    ON_EN_CHANGE(IDC_EDIT_NAME_PARAM, &CMainFrame::OnLiveTextPreviewChanged)
    ON_EN_CHANGE(IDC_EDIT_NAME_TMPL, &CMainFrame::OnLiveTextPreviewChanged)
    ON_EN_CHANGE(IDC_EDIT_DESC_PARAM, &CMainFrame::OnLiveTextPreviewChanged)
    ON_EN_CHANGE(IDC_EDIT_DESC_TMPL, &CMainFrame::OnLiveTextPreviewChanged)
    ON_BN_CLICKED(IDC_BTN_ALLOC_NAME_MID, &CMainFrame::OnAllocNameMid)
    ON_BN_CLICKED(IDC_BTN_ALLOC_DESC_MID, &CMainFrame::OnAllocDescMid)
    ON_BN_CLICKED(IDC_BTN_FORK_MIDS, &CMainFrame::OnForkExclusiveMids)
    ON_BN_CLICKED(IDC_BTN_SEARCH_NAME_MID, &CMainFrame::OnSearchNameUIString)
    ON_BN_CLICKED(IDC_BTN_SEARCH_DESC_MID, &CMainFrame::OnSearchDescUIString)
    ON_BN_CLICKED(IDC_BTN_GEN_TOOLTIP, &CMainFrame::OnGenerateRichTooltip)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_BTN_COLOR_FIRST, IDC_BTN_COLOR_LAST, &CMainFrame::OnInsertColorTag)
    ON_BN_CLICKED(IDC_BTN_APPLY_ARCHETYPE, &CMainFrame::OnStatsApplyArchetype)
    ON_BN_CLICKED(IDC_BTN_STATS_AUTOCURVE, &CMainFrame::OnStatsAutoCurve)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_BTN_STAT_SCALE_FIRST, IDC_BTN_STAT_SCALE_LAST, &CMainFrame::OnStatsScaleButton)
    ON_BN_CLICKED(IDC_BTN_STATS_SPREAD15, &CMainFrame::OnStatsSpread15)
    ON_BN_CLICKED(IDC_BTN_STATS_MIN_TO_MAX, &CMainFrame::OnStatsCopyMinToMax)
    ON_BN_CLICKED(IDC_BTN_STATS_PROBS_FULL, &CMainFrame::OnStatsProbsFull)
    ON_BN_CLICKED(IDC_BTN_STATS_COMPACT, &CMainFrame::OnStatsCompact)
    ON_BN_CLICKED(IDC_BTN_STATS_COPY, &CMainFrame::OnStatsCopyClipboard)
    ON_BN_CLICKED(IDC_BTN_STATS_PASTE, &CMainFrame::OnStatsPasteClipboard)
    ON_BN_CLICKED(IDC_BTN_STATS_CLEAR, &CMainFrame::OnStatsClearUnused)
    ON_CBN_SELCHANGE(IDC_CMB_RENDER_MODE, &CMainFrame::OnRenderModeChanged)
    ON_BN_CLICKED(IDC_CHK_AUTOSPIN, &CMainFrame::OnToggleAutoSpin)
    ON_BN_CLICKED(IDC_BTN_ZOOM_IN_3D, &CMainFrame::OnZoomIn3D)
    ON_BN_CLICKED(IDC_BTN_ZOOM_OUT_3D, &CMainFrame::OnZoomOut3D)
    ON_BN_CLICKED(IDC_BTN_RESET_CAMERA, &CMainFrame::OnResetCamera)
    ON_CBN_SELCHANGE(IDC_CMB_CHAR_CLASS, &CMainFrame::OnCharClassChanged)
    ON_CBN_SELCHANGE(IDC_CMB_CHAR_ACTION, &CMainFrame::OnCharActionChanged)
    ON_BN_CLICKED(IDC_BTN_ANIM_PLAY_PAUSE, &CMainFrame::OnAnimPlayPauseClicked)
    ON_BN_CLICKED(IDC_BTN_ANIM_RESET, &CMainFrame::OnAnimResetClicked)
    ON_BN_CLICKED(IDC_CHK_ANIM_LOOP, &CMainFrame::OnAnimLoopToggled)
    ON_CBN_SELCHANGE(IDC_CMB_ANIM_SPEED, &CMainFrame::OnAnimSpeedChanged)
    ON_BN_CLICKED(IDC_BTN_USE_SEL_DONOR, &CMainFrame::OnUseSelectedAsDonor)
    ON_BN_CLICKED(IDC_BTN_CLONE_VISUAL_BOTH, &CMainFrame::OnCopyVisualBoth)
    ON_BN_CLICKED(IDC_BTN_CLONE_SKIN_ONLY, &CMainFrame::OnCopyVisualSkinOnly)
    ON_BN_CLICKED(IDC_BTN_CLONE_ICON_ONLY, &CMainFrame::OnCopyVisualIconOnly)
    ON_CBN_SELCHANGE(IDC_RAW_SOURCE_COMBO, &CMainFrame::OnRawTableSourceChanged)
    ON_EN_CHANGE(IDC_RAW_FILTER_EDIT, &CMainFrame::OnRawFilterChanged)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_RAW_COL_LIST, &CMainFrame::OnRawListSelectionChanged)
    ON_NOTIFY(NM_DBLCLK, IDC_RAW_COL_LIST, &CMainFrame::OnRawListDblClick)
    ON_BN_CLICKED(IDC_RAW_WRITE_BTN, &CMainFrame::OnRawWriteCell)
    ON_BN_CLICKED(IDC_RAW_COPY_TSV_BTN, &CMainFrame::OnRawCopyRowTsv)
    ON_BN_CLICKED(IDC_RAW_CLONE_DETAIL_BTN, &CMainFrame::OnRawCloneDetailFromDonor)
    ON_BN_CLICKED(IDC_BTN_RUN_TIER_SET, &CMainFrame::OnGenerateTierSet)
    ON_BN_CLICKED(IDC_BTN_RUN_CLASS_SERIES, &CMainFrame::OnGenerateClassSeries)
    ON_BN_CLICKED(IDC_BTN_RUN_BATCH_EDIT, &CMainFrame::OnExecuteBatchEdit)
    ON_BN_CLICKED(IDC_BTN_JSON_EXP_SEL, &CMainFrame::OnExportSelectedJson)
    ON_BN_CLICKED(IDC_BTN_JSON_EXP_MOD, &CMainFrame::OnExportModifiedJson)
    ON_BN_CLICKED(IDC_BTN_JSON_EXP_FILT, &CMainFrame::OnExportFilteredJson)
    ON_BN_CLICKED(IDC_BTN_JSON_IMP_FILE, &CMainFrame::OnImportJsonFile)
    ON_BN_CLICKED(IDC_BTN_JSON_IMP_CLIP, &CMainFrame::OnImportJsonClipboard)
    ON_BN_CLICKED(IDC_BTN_MOD_REFRESH, &CMainFrame::OnRefreshModifiedList)
    ON_NOTIFY(NM_DBLCLK, IDC_MODIFIED_ITEMS_LIST, &CMainFrame::OnModifiedListDblClick)
END_MESSAGE_MAP()

CMainFrame::CMainFrame() {
    m_bAutoMenuEnable = FALSE;
}

CMainFrame::~CMainFrame() {
    if (m_loadWorker.joinable()) {
        m_loadWorker.join();
    }
    if (m_hAccel) {
        ::DestroyAcceleratorTable(m_hAccel);
        m_hAccel = nullptr;
    }
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
    if (!CFrameWnd::PreCreateWindow(cs)) return FALSE;
    cs.style &= ~FWS_ADDTOTITLE;
    cs.style |= WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    return TRUE;
}

bool CMainFrame::CreateMainWindow() {
    const int screenW = ::GetSystemMetrics(SM_CXSCREEN);
    const int screenH = ::GetSystemMetrics(SM_CYSCREEN);
    const int dpi = GetWindowDpi(nullptr);
    const int initW = std::min(DpiScale(1540, dpi), std::max(1080, screenW - 60));
    const int initH = std::min(DpiScale(920, dpi), std::max(720, screenH - 60));
    const int left = std::max(0, (screenW - initW) / 2);
    const int top = std::max(0, (screenH - initH) / 2);

    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
                                      ::LoadCursor(nullptr, IDC_ARROW),
                                      Theme::ChromeBrush(),
                                      AfxGetApp()->LoadIconW(IDI_APP_ICON));

    m_strTitle = Utf8ToWide(std::format("{} v{}", Resources::Version::AppName, Resources::Version::ShortVersionString));
    return CreateEx(0, cls, m_strTitle,
                    WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                    CRect(left, top, left + initW, top + initH),
                    nullptr, 0) != FALSE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1) return -1;

    m_hAppIcon = AfxGetApp()->LoadIconW(IDI_APP_ICON);
    if (m_hAppIcon) {
        SetIcon(m_hAppIcon, TRUE);
        SetIcon(m_hAppIcon, FALSE);
    }

    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_fonts.Build(dpi);

    BuildMenusAndAccelerators();

    static const UINT kIndicators[4] = { ID_SEPARATOR, ID_SEPARATOR, ID_SEPARATOR, ID_SEPARATOR };
    if (m_statusBar.Create(this)) {
        m_statusBar.SetIndicators(kIndicators, 4);
        m_statusBar.SetPaneInfo(0, ID_SEPARATOR, SBPS_STRETCH, DpiScale(420, dpi));
        m_statusBar.SetPaneInfo(1, ID_SEPARATOR, SBPS_NORMAL, DpiScale(230, dpi));
        m_statusBar.SetPaneInfo(2, ID_SEPARATOR, SBPS_NORMAL, DpiScale(210, dpi));
        m_statusBar.SetPaneInfo(3, ID_SEPARATOR, SBPS_NORMAL, DpiScale(250, dpi));
        m_statusBar.SetFont(&m_fonts.regular);
        // The themed status bar ignores colors; the classic one honors SB_SETBKCOLOR.
        ::SetWindowTheme(m_statusBar.GetSafeHwnd(), L"", L"");
        m_statusBar.GetStatusBarCtrl().SetBkColor(Theme::kChrome);
        for (int i = 0; i < 4; ++i) {
            m_statusBar.SetPaneStyle(i, m_statusBar.GetPaneStyle(i) | SBPS_NOBORDERS);
        }
    }

    CreateChildControls();

    wchar_t exeBuf[MAX_PATH] = {};
    ::GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
    const auto appDir = std::filesystem::path(exeBuf).parent_path();
    m_engine.InitializeServices(appDir);

    UpdateWindowCaption();
    UpdateStatusBar("Ready. Detecting Dragon Nest resource workspace...");

    return 0;
}

void CMainFrame::BuildMenusAndAccelerators() {
    CMenu mainMenu;
    mainMenu.CreateMenu();

    CMenu fileMenu;
    fileMenu.CreatePopupMenu();
    fileMenu.AppendMenuW(MF_STRING, ID_CMD_OPEN_WORKSPACE, L"&Open Resource Workspace...\tCtrl+O");
    fileMenu.AppendMenuW(MF_STRING, ID_CMD_RELOAD_WORKSPACE, L"&Reload Workspace\tF5");
    fileMenu.AppendMenuW(MF_SEPARATOR);
    fileMenu.AppendMenuW(MF_STRING, ID_CMD_SAVE_TO_FOLDER, L"&Save Modified to Folder...\tCtrl+S");
    fileMenu.AppendMenuW(MF_STRING, ID_CMD_COMMIT_DISK, L"&Commit Changes In-Place to Workspace...");
    fileMenu.AppendMenuW(MF_STRING, ID_CMD_DISCARD_DRAFTS, L"Clear SQLite Recovery &Drafts");
    fileMenu.AppendMenuW(MF_SEPARATOR);
    fileMenu.AppendMenuW(MF_STRING, ID_CMD_EXIT_APP, L"E&xit\tAlt+F4");
    mainMenu.AppendMenuW(MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu.Detach()), L"&File");

    CMenu itemMenu;
    itemMenu.CreatePopupMenu();
    itemMenu.AppendMenuW(MF_STRING, ID_CMD_ITEM_NEW, L"&New Item...\tCtrl+N");
    itemMenu.AppendMenuW(MF_STRING, ID_CMD_ITEM_DUPLICATE, L"&Duplicate Selected Item...\tCtrl+D");
    itemMenu.AppendMenuW(MF_SEPARATOR);
    itemMenu.AppendMenuW(MF_STRING, ID_CMD_ITEM_APPLY, L"&Apply Current Item Edits\tCtrl+Enter");
    itemMenu.AppendMenuW(MF_STRING, ID_CMD_ITEM_UNDO, L"&Undo Last Session Action\tCtrl+Z");
    itemMenu.AppendMenuW(MF_STRING, ID_CMD_ITEM_REVERT, L"Re&vert Form Fields");
    itemMenu.AppendMenuW(MF_STRING, ID_CMD_ITEM_DELETE, L"De&lete Selected Item");
    itemMenu.AppendMenuW(MF_SEPARATOR);
    itemMenu.AppendMenuW(MF_STRING, ID_CMD_ITEM_PICK_ICON, L"Browse &Icon Atlas...");
    itemMenu.AppendMenuW(MF_STRING, IDC_BTN_PROP_ICON_EDITOR, L"Custom &Icon Studio && Pixel Creator...");
    itemMenu.AppendMenuW(MF_STRING, ID_CMD_FOCUS_SEARCH, L"&Find Item...\tCtrl+F");
    mainMenu.AppendMenuW(MF_POPUP, reinterpret_cast<UINT_PTR>(itemMenu.Detach()), L"&Edit & Item");

    CMenu toolsMenu;
    toolsMenu.CreatePopupMenu();
    toolsMenu.AppendMenuW(MF_STRING, ID_CMD_TOOLS_CHARM_CALC, L"&Charm && Gacha Probability Calculator...\tCtrl+G");
    toolsMenu.AppendMenuW(MF_STRING, ID_CMD_TOOLS_DRAGON_JEWEL, L"&Dragon Jewel && Socket Studio...\tCtrl+J");
    toolsMenu.AppendMenuW(MF_STRING, ID_CMD_TOOLS_SET_ITEM, L"&Set Item Bonuses && Pieces...\tCtrl+Shift+S");
    toolsMenu.AppendMenuW(MF_STRING, ID_CMD_TOOLS_ENCHANT, L"&Enchantment Curve && Upgrade Simulator...\tCtrl+Shift+E");
    toolsMenu.AppendMenuW(MF_STRING, IDC_BTN_TOOLS_ACT_CONVERT, L"Convert &ACT v6 to v5 (Compatibility Tool)...");
    toolsMenu.AppendMenuW(MF_STRING, IDC_BTN_TOOLS_ICON_STUDIO, L"Custom &Item Icon Studio...");
    toolsMenu.AppendMenuW(MF_SEPARATOR);
    toolsMenu.AppendMenuW(MF_STRING, ID_CMD_WORKSPACE_AUDIT, L"Run Full Workspace &Integrity Audit...");
    toolsMenu.AppendMenuW(MF_STRING, ID_CMD_VERIFY_ROUNDTRIP, L"&Verify Binary DNT / UIString Round-Trip...");
    toolsMenu.AppendMenuW(MF_STRING, ID_CMD_CLEAR_CACHE, L"Clear &In-Memory Asset Cache");
    toolsMenu.AppendMenuW(MF_SEPARATOR);
    toolsMenu.AppendMenuW(MF_STRING, ID_CMD_CAPTURE_SNAPSHOT, L"Capture Diagnostic &Snapshot (.dmp + Crash Log)");
    toolsMenu.AppendMenuW(MF_STRING, ID_CMD_OPEN_LOGS, L"Open &Logs Directory");
    mainMenu.AppendMenuW(MF_POPUP, reinterpret_cast<UINT_PTR>(toolsMenu.Detach()), L"&Tools & Audit");

    CMenu helpMenu;
    helpMenu.CreatePopupMenu();
    helpMenu.AppendMenuW(MF_STRING, ID_CMD_HELP_ABOUT, L"&About Dragon Nest Item Maker...");
    mainMenu.AppendMenuW(MF_POPUP, reinterpret_cast<UINT_PTR>(helpMenu.Detach()), L"&Help");

    SetMenu(&mainMenu);
    mainMenu.Detach();

    ACCEL accels[] = {
        { FVIRTKEY | FCONTROL, 'O', ID_CMD_OPEN_WORKSPACE },
        { FVIRTKEY, VK_F5, ID_CMD_RELOAD_WORKSPACE },
        { FVIRTKEY | FCONTROL, 'S', ID_CMD_SAVE_TO_FOLDER },
        { FVIRTKEY | FCONTROL, 'N', ID_CMD_ITEM_NEW },
        { FVIRTKEY | FCONTROL, 'D', ID_CMD_ITEM_DUPLICATE },
        { FVIRTKEY | FCONTROL, 'Z', ID_CMD_ITEM_UNDO },
        { FVIRTKEY | FCONTROL, VK_RETURN, ID_CMD_ITEM_APPLY },
        { FVIRTKEY | FCONTROL, 'F', ID_CMD_FOCUS_SEARCH },
        { FVIRTKEY | FCONTROL, '1', ID_CMD_TAB_1 },
        { FVIRTKEY | FCONTROL, '2', ID_CMD_TAB_2 },
        { FVIRTKEY | FCONTROL, '3', ID_CMD_TAB_3 },
        { FVIRTKEY | FCONTROL, '4', ID_CMD_TAB_4 },
        { FVIRTKEY | FCONTROL, '5', ID_CMD_TAB_5 },
        { FVIRTKEY | FCONTROL, '6', ID_CMD_TAB_6 },
        { FVIRTKEY | FCONTROL, '7', ID_CMD_TAB_7 },
        { FVIRTKEY | FCONTROL, '8', ID_CMD_TAB_8 },
        { FVIRTKEY | FCONTROL, 'G', ID_CMD_TOOLS_CHARM_CALC },
        { FVIRTKEY | FCONTROL, 'J', ID_CMD_TOOLS_DRAGON_JEWEL },
        { FVIRTKEY | FCONTROL | FSHIFT, 'S', ID_CMD_TOOLS_SET_ITEM },
        { FVIRTKEY | FCONTROL | FSHIFT, 'E', ID_CMD_TOOLS_ENCHANT },
    };
    m_hAccel = ::CreateAcceleratorTableW(accels, static_cast<int>(std::size(accels)));
}

void CMainFrame::CreateChildControls() {
    const CRect z(0, 0, 10, 10);
    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_btnHdrOpen.Create(L"Open Workspace...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_OPEN_WORKSPACE);
    m_btnHdrNew.Create(L"+ New Item", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_NEW);
    m_btnHdrDup.Create(L"Duplicate", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_DUPLICATE);
    m_btnHdrUndo.Create(L"Undo", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_UNDO);
    m_btnHdrSaveToFolder.Create(L"Save to Folder... (Ctrl+S)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_SAVE_TO_FOLDER);
    m_btnHdrCommit.Create(L"Commit In-Place...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_COMMIT_DISK);
    m_btnHdrAudit.Create(L"Audit DNT", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_WORKSPACE_AUDIT);
    m_btnHdrVerify.Create(L"Verify 1:1", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_VERIFY_ROUNDTRIP);
    m_edtSearch.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_SEARCH_EDIT);
    m_edtSearch.SendMessageW(EM_SETCUEBANNER, TRUE,
                             reinterpret_cast<LPARAM>(L"Search name, #ID, mid:123, lv>=95, job:1 ..."));
    m_btnClearSearch.Create(L"Clear", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_SEARCH_CLEAR_BTN);

    m_cmbTableFilter.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, z, this, IDC_TABLE_FILTER_COMBO);
    m_cmbTableFilter.AddString(L"All Item Tables (itemtable*.dnt)");
    m_cmbTableFilter.SetCurSel(0);

    m_cmbGroupFilter.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, z, this, IDC_GROUP_FILTER_COMBO);
    for (const wchar_t* g : { L"All Groups", L"Weapons", L"Armor / Parts", L"Accessories", L"General / Other" }) {
        m_cmbGroupFilter.AddString(g);
    }
    m_cmbGroupFilter.SetCurSel(0);

    m_cmbRankFilter.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, z, this, IDC_RANK_FILTER_COMBO);
    for (const wchar_t* r : { L"All Ranks", L"0 - Normal (D)", L"1 - Magic (C)", L"2 - Rare (B)",
                              L"3 - Epic (A)", L"4 - Unique (S)", L"5 - Heroic (SS)", L"6 - Named (SSS)" }) {
        m_cmbRankFilter.AddString(r);
    }
    m_cmbRankFilter.SetCurSel(0);

    m_chkModifiedOnly.Create(L"Modified Only", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_CHK_MODIFIED_ONLY);

    m_lstItems.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_OWNERDATA |
                          LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                      z, this, IDC_ITEM_LIST);
    m_lstItems.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    m_lstItems.InsertColumn(0, L"ID", LVCFMT_RIGHT, DpiScale(82, dpi));
    m_lstItems.InsertColumn(1, L"Item Name", LVCFMT_LEFT, DpiScale(195, dpi));
    m_lstItems.InsertColumn(2, L"Rank", LVCFMT_LEFT, DpiScale(64, dpi));
    m_lstItems.InsertColumn(3, L"Lv", LVCFMT_RIGHT, DpiScale(40, dpi));
    m_lstItems.InsertColumn(4, L"Type / Slot", LVCFMT_LEFT, DpiScale(92, dpi));
    m_lstItems.InsertColumn(5, L"Source DNT", LVCFMT_LEFT, DpiScale(135, dpi));

    m_lblMatchCount.Create(L"0 items", WS_CHILD | WS_VISIBLE, z, this);
    m_btnNewItem.Create(L"+ New Item", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_NEW);
    m_btnDupItem.Create(L"Duplicate", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_DUPLICATE);
    m_btnDelItem.Create(L"Delete", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_DELETE);
    m_grpBrowser.Create(L"Item Browser", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, z, this, IDC_STATIC);
    m_grpSummary.Create(L"Selected Item Summary", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, z, this, IDC_STATIC);
    m_bannerIconWnd.Create(this, z, IDC_BANNER_ICON, ID_CMD_ITEM_PICK_ICON);
    m_lblBannerName.Create(L"No item selected", WS_CHILD | WS_VISIBLE, z, this);
    m_lblBannerName.SetFont(&m_fonts.bold);
    m_lblBannerMeta.Create(L"", WS_CHILD | WS_VISIBLE, z, this);
    m_lblBannerMeta.SetFont(&m_fonts.regular);
    m_lblBannerMid.Create(L"", WS_CHILD | WS_VISIBLE, z, this);
    m_lblBannerMid.SetFont(&m_fonts.secondary);
    m_btnBannerPickIcon.Create(L"Browse Icon...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_PICK_ICON);

    m_tabCtrl.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_TABS, z, this, IDC_TAB_CTRL);
    m_visibleTabs = { 0, 1, 4, 5, 6 };

    auto regTab = [&](int tab, CWnd& w) {
        m_tabControls[static_cast<size_t>(tab)].push_back(&w);
    };

    m_grpBasicProps.Create(L"Core Equipment & Economy Attributes", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(0, m_grpBasicProps);
    m_grpJobs.Create(L"Job Class & Equippable Matrix", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(0, m_grpJobs);

    m_grpNameMid.Create(L"Item Name (UIString MID)", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(1, m_grpNameMid);
    m_grpColorTags.Create(L"Quick Color Tag Helpers", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(1, m_grpColorTags);
    m_grpDescMid.Create(L"Item Description (UIString MID)", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(1, m_grpDescMid);

    m_grpArchetypes.Create(L"Stat Archetypes & Automated Math Scaling", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(2, m_grpArchetypes);
    m_grpStateRows.Create(L"Direct State Slots (State1 - State10)", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(2, m_grpStateRows);

    m_grpViewport.Create(L"3D Model & Ground Plane Viewport", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(3, m_grpViewport);
    m_grpVisualCloner.Create(L"Visual Appearance Donor & Cloner", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(3, m_grpVisualCloner);

    m_grpRawInspector.Create(L"Table Inspector & Direct Field Editor", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(4, m_grpRawInspector);
    m_grpRawEdit.Create(L"Cell Value Editor", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(4, m_grpRawEdit);

    m_grpTierGen.Create(L"Tier Set Generator", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(5, m_grpTierGen);
    m_grpSeriesGen.Create(L"Job Class Series Generator", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(5, m_grpSeriesGen);
    m_grpBatchEdit.Create(L"Mass Multi-Item Batch Editor", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(5, m_grpBatchEdit);

    m_grpTools.Create(L"Diagnostics, Integrity & Caches", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(6, m_grpTools);
    m_grpJsonPacks.Create(L"JSON Item Packs", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(6, m_grpJsonPacks);
    m_grpModDrafts.Create(L"Active Session Modified Items & Drafts", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(6, m_grpModDrafts);

    // Tab 7 (Charm / Gacha Box Probability Studio)
    m_grpCharmHeader.Create(L"Charm & Drop Pool Selection", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(7, m_grpCharmHeader);
    m_grpCharmTable.Create(L"Drop Pool Table & Odds Matrix", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(7, m_grpCharmTable);
    m_grpCharmStats.Create(L"Drop Analytics & Statistical Pity Lines", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(7, m_grpCharmStats);
    m_grpCharmSim.Create(L"Monte Carlo Gacha Simulator", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(7, m_grpCharmSim);
    m_grpCharmBalance.Create(L"Drop Weight Rebalance & Tuning Tool", WS_CHILD | BS_GROUPBOX, z, this, IDC_STATIC); regTab(7, m_grpCharmBalance);

    m_lblCharmSelector.Create(L"Drop Set / Charm Group:", WS_CHILD, z, this); regTab(7, m_lblCharmSelector);
    m_cmbCharmSelector.Create(WS_CHILD | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, z, this, IDC_CHARM_SELECTOR); regTab(7, m_cmbCharmSelector);
    m_btnCharmSyncItem.Create(L"Sync to Current Item", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_CHARM_SYNC_ITEM); regTab(7, m_btnCharmSyncItem);
    m_btnCharmRefresh.Create(L"Refresh", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_CHARM_REFRESH); regTab(7, m_btnCharmRefresh);
    m_btnCharmExportCsv.Create(L"Export CSV...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_CHARM_EXPORT_CSV); regTab(7, m_btnCharmExportCsv);

    m_lstCharmRewards.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, z, this, IDC_LST_CHARM_REWARDS); regTab(7, m_lstCharmRewards);
    m_lstCharmRewards.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    m_lstCharmRewards.InsertColumn(0, L"#", LVCFMT_RIGHT, DpiScale(32, dpi));
    m_lstCharmRewards.InsertColumn(1, L"Row ID", LVCFMT_RIGHT, DpiScale(58, dpi));
    m_lstCharmRewards.InsertColumn(2, L"Item ID", LVCFMT_RIGHT, DpiScale(72, dpi));
    m_lstCharmRewards.InsertColumn(3, L"Reward Item Name", LVCFMT_LEFT, DpiScale(175, dpi));
    m_lstCharmRewards.InsertColumn(4, L"Rank", LVCFMT_LEFT, DpiScale(64, dpi));
    m_lstCharmRewards.InsertColumn(5, L"Count", LVCFMT_RIGHT, DpiScale(44, dpi));
    m_lstCharmRewards.InsertColumn(6, L"Prob Weight", LVCFMT_RIGHT, DpiScale(74, dpi));
    m_lstCharmRewards.InsertColumn(7, L"Drop %", LVCFMT_RIGHT, DpiScale(64, dpi));
    m_lstCharmRewards.InsertColumn(8, L"1 in N Odds", LVCFMT_RIGHT, DpiScale(80, dpi));
    m_lstCharmRewards.InsertColumn(9, L"PotenProb", LVCFMT_RIGHT, DpiScale(64, dpi));
    m_lstCharmRewards.InsertColumn(10, L"Notice", LVCFMT_CENTER, DpiScale(54, dpi));
    m_lstCharmRewards.InsertColumn(11, L"Gold", LVCFMT_RIGHT, DpiScale(54, dpi));

    m_lblCharmStatSummary.Create(L"", WS_CHILD, z, this); regTab(7, m_lblCharmStatSummary);
    m_lblCharmStatOdds.Create(L"", WS_CHILD, z, this); regTab(7, m_lblCharmStatOdds);

    m_lblSimTitle.Create(L"Simulate Pulls:", WS_CHILD, z, this); regTab(7, m_lblSimTitle);
    m_btnSimPull1.Create(L"1x", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_SIM_PULL_1); regTab(7, m_btnSimPull1);
    m_btnSimPull10.Create(L"10x", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_SIM_PULL_10); regTab(7, m_btnSimPull10);
    m_btnSimPull100.Create(L"100x", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_SIM_PULL_100); regTab(7, m_btnSimPull100);
    m_btnSimPull1000.Create(L"1,000x", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_SIM_PULL_1000); regTab(7, m_btnSimPull1000);
    m_btnSimJackpot.Create(L"Until Jackpot", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_SIM_JACKPOT); regTab(7, m_btnSimJackpot);
    m_btnSimReset.Create(L"Reset Stats", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_SIM_RESET); regTab(7, m_btnSimReset);
    m_edtSimLog.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, z, this, IDC_EDIT_SIM_LOG); regTab(7, m_edtSimLog);

    m_lblCharmTargetWeight.Create(L"Normalize Target Total:", WS_CHILD, z, this); regTab(7, m_lblCharmTargetWeight);
    m_edtCharmTargetWeight.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, z, this, IDC_EDIT_CHARM_TARGET_WEIGHT); regTab(7, m_edtCharmTargetWeight);
    m_edtCharmTargetWeight.SetWindowTextW(L"100000");
    m_btnCharmNormalize.Create(L"Normalize Pool Weights", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_CHARM_NORMALIZE); regTab(7, m_btnCharmNormalize);

    m_lblCharmEditWeight.Create(L"Selected Reward Weight:", WS_CHILD, z, this); regTab(7, m_lblCharmEditWeight);
    m_edtCharmEditWeight.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, z, this, IDC_EDIT_CHARM_EDIT_WEIGHT); regTab(7, m_edtCharmEditWeight);
    m_btnCharmApplyWeight.Create(L"Apply New Weight", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_CHARM_APPLY_WEIGHT); regTab(7, m_btnCharmApplyWeight);

    m_grpBottomActions.Create(L"Item & Workspace Actions", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, z, this, IDC_STATIC);
    m_btnSaveToFolder.Create(L"Save to Folder... (Ctrl+S)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_SAVE_TO_FOLDER);
    m_lblRank.Create(L"Rank (_Rank):", WS_CHILD, z, this); regTab(0, m_lblRank);
    m_cmbEditRank.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, z, this, IDC_EDIT_RANK_COMBO);
    for (const wchar_t* r : { L"0 - Normal (D)", L"1 - Magic (C)", L"2 - Rare (B)", L"3 - Epic (A)",
                              L"4 - Unique (S)", L"5 - Heroic (SS)", L"6 - Named (SSS)" }) {
        m_cmbEditRank.AddString(r);
    }
    regTab(0, m_cmbEditRank);

    m_lblIconIdx.Create(L"Icon Index:", WS_CHILD, z, this); regTab(0, m_lblIconIdx);
    m_edtIconIndex.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, z, this, IDC_EDIT_ICON_INDEX); regTab(0, m_edtIconIndex);
    m_btnBrowseIcon.Create(L"Pick Icon...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_PICK_ICON); regTab(0, m_btnBrowseIcon);
    m_btnIconEditor.Create(L"Icon Studio...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_PROP_ICON_EDITOR); regTab(0, m_btnIconEditor);

    m_lblLevel.Create(L"Level Limit:", WS_CHILD, z, this); regTab(0, m_lblLevel);
    m_edtLevel.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, z, this, IDC_EDIT_LEVEL); regTab(0, m_edtLevel);

    m_lblHeroLevel.Create(L"Hero Level:", WS_CHILD, z, this); regTab(0, m_lblHeroLevel);
    m_edtHeroLevel.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, z, this, IDC_EDIT_HERO_LEVEL); regTab(0, m_edtHeroLevel);

    m_lblOverlap.Create(L"Max Stack:", WS_CHILD, z, this); regTab(0, m_lblOverlap);
    m_edtOverlap.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, z, this, IDC_EDIT_OVERLAP); regTab(0, m_edtOverlap);

    m_lblSell.Create(L"Sell Copper:", WS_CHILD, z, this); regTab(0, m_lblSell);
    m_edtSell.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_EDIT_SELL); regTab(0, m_edtSell);
    m_btnAutoSell.Create(L"Auto", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_AUTO_SELL); regTab(0, m_btnAutoSell);

    m_lblGearScore.Create(L"Gear Score:", WS_CHILD, z, this); regTab(0, m_lblGearScore);
    m_edtGearScore.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_EDIT_GEAR_SCORE); regTab(0, m_edtGearScore);
    m_btnAutoGearScore.Create(L"Auto", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_AUTO_GS); regTab(0, m_btnAutoGearScore);

    m_lblDurability.Create(L"Durability:", WS_CHILD, z, this); regTab(0, m_lblDurability);
    m_edtDurability.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, z, this, IDC_EDIT_DURABILITY); regTab(0, m_edtDurability);
    m_btnAutoDurability.Create(L"Auto", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_AUTO_DURA); regTab(0, m_btnAutoDurability);

    m_btnAutoAllEconomy.Create(L"Auto-Calculate Gear Score, Sell Price & Durability", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_AUTO_ALL_ECON);
    regTab(0, m_btnAutoAllEconomy);

    m_chkCash.Create(L"Cash Item (_IsCash)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_CHK_CASH); regTab(0, m_chkCash);
    m_chkStorage.Create(L"General Storage (_AbleStorage)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_CHK_STORAGE); regTab(0, m_chkStorage);
    m_chkWStorage.Create(L"Server Storage (_AbleWStorage)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_CHK_WSTORAGE); regTab(0, m_chkWStorage);

    m_btnPresetEquip.Create(L"Template: Standard Equip", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_PRESET_EQUIP); regTab(0, m_btnPresetEquip);
    m_btnPresetStack.Create(L"Template: Stackable x999", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_PRESET_STACK); regTab(0, m_btnPresetStack);
    m_btnPresetCash.Create(L"Template: Cash Bound", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_PRESET_CASH); regTab(0, m_btnPresetCash);

    m_lblPropJewelInfo.Create(L"Dragon Jewel Sockets: None", WS_CHILD | SS_ENDELLIPSIS | SS_NOPREFIX, z, this); regTab(0, m_lblPropJewelInfo);
    m_btnPropJewelStudio.Create(L"Dragon Jewel Studio...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_PROP_DRAGON_JEWEL); regTab(0, m_btnPropJewelStudio);

    m_lblPropSetItemInfo.Create(L"Set Item: None", WS_CHILD | SS_ENDELLIPSIS | SS_NOPREFIX, z, this); regTab(0, m_lblPropSetItemInfo);
    m_btnPropSetItemStudio.Create(L"Set Item Studio...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_PROP_SET_ITEM); regTab(0, m_btnPropSetItemStudio);

    m_lblPropEnchantInfo.Create(L"Enchant Curve: None", WS_CHILD | SS_ENDELLIPSIS | SS_NOPREFIX, z, this); regTab(0, m_lblPropEnchantInfo);
    m_btnPropEnchantStudio.Create(L"Enchant Studio...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_PROP_ENCHANT); regTab(0, m_btnPropEnchantStudio);

    m_lblJobs.Create(L"Raw _NeedJobClass:", WS_CHILD, z, this); regTab(0, m_lblJobs);
    m_edtJobs.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_EDIT_JOBS); regTab(0, m_edtJobs);
    m_lblJobsResolved.Create(L"All Classes", WS_CHILD | SS_ENDELLIPSIS | SS_NOPREFIX, z, this); regTab(0, m_lblJobsResolved);

    m_chkClassAll.Create(L"0 - All Classes (Unrestricted)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_CHK_CLASS_ALL);
    regTab(0, m_chkClassAll);
    for (size_t i = 0; i < 10; ++i) {
        m_chkBaseClasses[i].Create(kBaseClassLabels[i], WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this,
                                   IDC_CHK_CLASS_BASE_FIRST + static_cast<UINT>(i));
        regTab(0, m_chkBaseClasses[i]);
    }

    m_lblSubclasses.Create(L"Subclasses (Specializations):", WS_CHILD | SS_ENDELLIPSIS | SS_NOPREFIX, z, this);
    regTab(0, m_lblSubclasses);
    m_btnSubclassAll1st.Create(L"All 1st", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_SUBCLASS_ALL_1ST);
    regTab(0, m_btnSubclassAll1st);
    m_btnSubclassAll2nd.Create(L"All 2nd", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_SUBCLASS_ALL_2ND);
    regTab(0, m_btnSubclassAll2nd);
    m_btnSubclassClear.Create(L"Clear", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_SUBCLASS_CLEAR);
    regTab(0, m_btnSubclassClear);
    m_btnSubclassExpand.Create(L"Collapse", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_SUBCLASS_EXPAND);
    regTab(0, m_btnSubclassExpand);

    m_treeSubclasses.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_TABSTOP |
                            TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_CHECKBOXES,
                            z, this, IDC_TREE_SUBCLASSES);
    m_treeSubclasses.ModifyStyle(0, TVS_CHECKBOXES);
    ::SetWindowTheme(m_treeSubclasses.m_hWnd, L"Explorer", nullptr);
    regTab(0, m_treeSubclasses);
    PopulateSubclassesTree();

    m_lblMidCollisionInfo.Create(L"UIString MID Reference Status", WS_CHILD | SS_ENDELLIPSIS | SS_NOPREFIX, z, this);
    regTab(1, m_lblMidCollisionInfo);
    m_btnForkMids.Create(L"Fork Exclusive MIDs (Safe Rename)", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_FORK_MIDS);
    regTab(1, m_btnForkMids);
    m_btnGenTooltip.Create(L"Auto-Generate Rich Tooltip", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_GEN_TOOLTIP);
    regTab(1, m_btnGenTooltip);

    m_lblNameMid.Create(L"Name MID:", WS_CHILD, z, this); regTab(1, m_lblNameMid);
    m_edtNameMid.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, z, this, IDC_EDIT_NAME_MID); regTab(1, m_edtNameMid);
    m_btnAllocNameMid.Create(L"New MID", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_ALLOC_NAME_MID); regTab(1, m_btnAllocNameMid);
    m_btnSearchNameMid.Create(L"Search UIString...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_SEARCH_NAME_MID); regTab(1, m_btnSearchNameMid);
    m_lblNameParam.Create(L"Params:", WS_CHILD, z, this); regTab(1, m_lblNameParam);
    m_edtNameParam.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_EDIT_NAME_PARAM); regTab(1, m_edtNameParam);

    m_lblNameTmpl.Create(L"Name Template:", WS_CHILD, z, this); regTab(1, m_lblNameTmpl);
    m_edtNameTmpl.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_EDIT_NAME_TMPL); regTab(1, m_edtNameTmpl);

    m_lblNameResolvedHdr.Create(L"Resolved Name:", WS_CHILD, z, this); regTab(1, m_lblNameResolvedHdr);
    m_edtNameResolved.Create(WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, z, this, IDC_EDIT_NAME_RESOLVED); regTab(1, m_edtNameResolved);

    m_lblDescMid.Create(L"Desc MID:", WS_CHILD, z, this); regTab(1, m_lblDescMid);
    m_edtDescMid.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, z, this, IDC_EDIT_DESC_MID); regTab(1, m_edtDescMid);
    m_btnAllocDescMid.Create(L"New MID", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_ALLOC_DESC_MID); regTab(1, m_btnAllocDescMid);
    m_btnSearchDescMid.Create(L"Search UIString...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_SEARCH_DESC_MID); regTab(1, m_btnSearchDescMid);
    m_lblDescParam.Create(L"Params:", WS_CHILD, z, this); regTab(1, m_lblDescParam);
    m_edtDescParam.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_EDIT_DESC_PARAM); regTab(1, m_edtDescParam);

    constexpr const wchar_t* kColorBtnLabels[7] = {
        L"#y Gold", L"#r Red", L"#b Blue", L"#g Green", L"#v Purple", L"#w Reset", L"\\n Line"
    };
    for (size_t i = 0; i < 7; ++i) {
        m_btnColorTags[i].Create(kColorBtnLabels[i], WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_COLOR_FIRST + static_cast<UINT>(i));
        regTab(1, m_btnColorTags[i]);
    }

    m_lblDescTmpl.Create(L"Description Template (uistring.xml):", WS_CHILD, z, this); regTab(1, m_lblDescTmpl);
    m_edtDescTmpl.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                         z, this, IDC_EDIT_DESC_TMPL); regTab(1, m_edtDescTmpl);

    m_lblDescResolvedHdr.Create(L"Live Resolved Tooltip Preview:", WS_CHILD, z, this); regTab(1, m_lblDescResolvedHdr);
    m_edtDescResolved.Create(WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                             z, this, IDC_EDIT_DESC_RESOLVED); regTab(1, m_edtDescResolved);
    m_lblArchetype.Create(L"Role Preset:", WS_CHILD, z, this); regTab(2, m_lblArchetype);
    m_cmbArchetype.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, z, this, IDC_CMB_ARCHETYPE);
    for (const wchar_t* a : { L"Physical DPS (PATK / STR / AGI / CRIT / FD)",
                              L"Magical DPS (MATK / INT / VIT / CRIT / FD)",
                              L"Tank & Vitality (DEF / MDEF / VIT / HP%)",
                              L"Critical & Final Damage Burst",
                              L"Elemental Hybrid (Fire / Ice / Light / Dark)",
                              L"Balanced All-Stat Omni Gear" }) {
        m_cmbArchetype.AddString(a);
    }
    m_cmbArchetype.SetCurSel(0);
    regTab(2, m_cmbArchetype);

    m_btnApplyArchetype.Create(L"Apply Role Preset", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_APPLY_ARCHETYPE); regTab(2, m_btnApplyArchetype);
    m_btnStatsAutoCurve.Create(L"Auto-Scale by Lv & Rank", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_STATS_AUTOCURVE); regTab(2, m_btnStatsAutoCurve);

    constexpr const wchar_t* kScaleLabels[5] = { L"+10%", L"+25%", L"+50%", L"x2.0", L"-20%" };
    for (size_t i = 0; i < 5; ++i) {
        m_btnStatScales[i].Create(kScaleLabels[i], WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_STAT_SCALE_FIRST + static_cast<UINT>(i));
        regTab(2, m_btnStatScales[i]);
    }
    m_btnStatsSpread15.Create(L"Max = Min+15%", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_STATS_SPREAD15); regTab(2, m_btnStatsSpread15);
    m_btnStatsMinToMax.Create(L"Max = Min", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_STATS_MIN_TO_MAX); regTab(2, m_btnStatsMinToMax);
    m_btnStatsProbsFull.Create(L"Probs = 1.0", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_STATS_PROBS_FULL); regTab(2, m_btnStatsProbsFull);
    m_btnStatsCompact.Create(L"Compact", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_STATS_COMPACT); regTab(2, m_btnStatsCompact);
    m_btnStatsCopy.Create(L"Copy Stats", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_STATS_COPY); regTab(2, m_btnStatsCopy);
    m_btnStatsPaste.Create(L"Paste", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_STATS_PASTE); regTab(2, m_btnStatsPaste);
    m_btnStatsClear.Create(L"Clear", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_STATS_CLEAR); regTab(2, m_btnStatsClear);

    m_lblStatHdrSlot.Create(L"Slot", WS_CHILD, z, this); regTab(2, m_lblStatHdrSlot);
    m_lblStatHdrType.Create(L"Attribute Type (_State1 .. _State10)", WS_CHILD, z, this); regTab(2, m_lblStatHdrType);
    m_lblStatHdrProb.Create(L"GenProb", WS_CHILD, z, this); regTab(2, m_lblStatHdrProb);
    m_lblStatHdrMin.Create(L"Min Value", WS_CHILD, z, this); regTab(2, m_lblStatHdrMin);
    m_lblStatHdrMax.Create(L"Max Value", WS_CHILD, z, this); regTab(2, m_lblStatHdrMax);

    const auto& stateNames = Data::ItemManager::StateTypeNames();
    for (size_t i = 0; i < Data::kStateSlotCount; ++i) {
        auto& row = m_statWidgets[i];
        const UINT baseId = IDC_STAT_BASE + static_cast<UINT>(i * 4);
        row.lblSlot.Create(Utf8ToWide(std::format("#{}", i + 1)), WS_CHILD, z, this);
        row.cmbType.Create(WS_CHILD | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, z, this, baseId);
        for (const auto& sn : stateNames) {
            row.cmbType.AddString(Utf8ToWide(sn));
        }
        row.cmbType.SetCurSel(0);
        row.edtProb.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, baseId + 1);
        row.edtMin.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, baseId + 2);
        row.edtMax.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, baseId + 3);

        regTab(2, row.lblSlot);
        regTab(2, row.cmbType);
        regTab(2, row.edtProb);
        regTab(2, row.edtMin);
        regTab(2, row.edtMax);
    }
    m_lblRenderMode.Create(L"Shading:", WS_CHILD, z, this); regTab(3, m_lblRenderMode);
    m_cmbRenderMode.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, z, this, IDC_CMB_RENDER_MODE);
    m_cmbRenderMode.AddString(L"Textured Lit (.dds + Lambert + Emissive)");
    m_cmbRenderMode.AddString(L"Clay Shaded (Geometry Normals)");
    m_cmbRenderMode.AddString(L"Wireframe Topology");
    m_cmbRenderMode.SetCurSel(0);
    regTab(3, m_cmbRenderMode);

    m_chkAutoSpin.Create(L"Auto-Spin 60 FPS", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_CHK_AUTOSPIN); regTab(3, m_chkAutoSpin);
    m_btnZoomIn3D.Create(L"Zoom +", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_ZOOM_IN_3D); regTab(3, m_btnZoomIn3D);
    m_btnZoomOut3D.Create(L"Zoom -", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_ZOOM_OUT_3D); regTab(3, m_btnZoomOut3D);
    m_btnResetCam.Create(L"Reset Camera", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_RESET_CAMERA); regTab(3, m_btnResetCam);
    m_btnOpenIconAtlas.Create(L"Open 2D Icon Atlas...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_PICK_ICON); regTab(3, m_btnOpenIconAtlas);
    m_btnViewportActConvert.Create(L"Convert ACT v6->v5...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_TOOLS_ACT_CONVERT); regTab(3, m_btnViewportActConvert);

    m_lblCharClass.Create(L"Character Class:", WS_CHILD, z, this); regTab(3, m_lblCharClass);
    m_cmbCharClass.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, z, this, IDC_CMB_CHAR_CLASS);
    for (const wchar_t* clsName : { L"Item Only (None)", L"1. Warrior", L"2. Archer", L"3. Sorceress",
                                    L"4. Cleric", L"5. Academic", L"6. Kali", L"7. Assassin",
                                    L"8. Lancea", L"9. Machina", L"10. Vandar" }) {
        m_cmbCharClass.AddString(clsName);
    }
    m_cmbCharClass.SetCurSel(0);
    regTab(3, m_cmbCharClass);

    m_lblCharAction.Create(L"Action:", WS_CHILD, z, this); regTab(3, m_lblCharAction);
    m_cmbCharAction.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, z, this, IDC_CMB_CHAR_ACTION);
    m_cmbCharAction.AddString(L"Stand");
    m_cmbCharAction.SetCurSel(0);
    regTab(3, m_cmbCharAction);

    m_btnAnimPlayPause.Create(L"⏸ Pause", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_ANIM_PLAY_PAUSE);
    regTab(3, m_btnAnimPlayPause);

    m_btnAnimReset.Create(L"⏮ Reset", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_ANIM_RESET);
    regTab(3, m_btnAnimReset);

    m_chkAnimLoop.Create(L"Loop", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_CHK_ANIM_LOOP);
    m_chkAnimLoop.SetCheck(BST_CHECKED);
    regTab(3, m_chkAnimLoop);

    m_lblAnimSpeed.Create(L"Speed:", WS_CHILD, z, this); regTab(3, m_lblAnimSpeed);
    m_cmbAnimSpeed.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, z, this, IDC_CMB_ANIM_SPEED);
    for (const wchar_t* spd : { L"0.25x", L"0.5x", L"1.0x", L"1.5x", L"2.0x" }) {
        m_cmbAnimSpeed.AddString(spd);
    }
    m_cmbAnimSpeed.SetCurSel(2); // 1.0x
    regTab(3, m_cmbAnimSpeed);

    m_previewWnd.Create(this, z, IDC_PREVIEW_WND);
    m_previewWnd.SetWorkspaceEngine(&m_engine);
    m_previewWnd.ShowWindow(SW_HIDE);
    regTab(3, m_previewWnd);

    m_lblDonorId.Create(L"Visual Donor #ID:", WS_CHILD, z, this); regTab(3, m_lblDonorId);
    m_edtDonorId.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_EDIT_DONOR_ID);
    m_edtDonorId.SendMessageW(EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Donor item #ID"));
    regTab(3, m_edtDonorId);
    m_btnUseSelAsDonor.Create(L"Set Current as Donor", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_USE_SEL_DONOR); regTab(3, m_btnUseSelAsDonor);
    m_btnCloneVisualBoth.Create(L"Reskin 3D + Icon", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_CLONE_VISUAL_BOTH); regTab(3, m_btnCloneVisualBoth);
    m_btnCloneSkinOnly.Create(L"Clone 3D Skin Only", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_CLONE_SKIN_ONLY); regTab(3, m_btnCloneSkinOnly);
    m_btnCloneIconOnly.Create(L"Clone 2D Icon Only", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_CLONE_ICON_ONLY); regTab(3, m_btnCloneIconOnly);
    m_lblRawSource.Create(L"Table:", WS_CHILD, z, this); regTab(4, m_lblRawSource);
    m_cmbRawSource.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, z, this, IDC_RAW_SOURCE_COMBO); regTab(4, m_cmbRawSource);
    m_edtRawFilter.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_RAW_FILTER_EDIT);
    m_edtRawFilter.SendMessageW(EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Filter column name..."));
    regTab(4, m_edtRawFilter);
    m_btnRawCopyTsv.Create(L"Copy Row TSV", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_RAW_COPY_TSV_BTN); regTab(4, m_btnRawCopyTsv);
    m_btnRawCloneDetail.Create(L"Clone Detail From Donor #ID", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_RAW_CLONE_DETAIL_BTN); regTab(4, m_btnRawCloneDetail);

    m_lstRawCols.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                        z, this, IDC_RAW_COL_LIST);
    m_lstRawCols.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    m_lstRawCols.InsertColumn(0, L"#", LVCFMT_RIGHT, DpiScale(44, dpi));
    m_lstRawCols.InsertColumn(1, L"Column Name", LVCFMT_LEFT, DpiScale(220, dpi));
    m_lstRawCols.InsertColumn(2, L"DNT Type", LVCFMT_LEFT, DpiScale(95, dpi));
    m_lstRawCols.InsertColumn(3, L"Raw Cell Value", LVCFMT_LEFT, DpiScale(320, dpi));
    regTab(4, m_lstRawCols);

    m_lblRawSelectedCol.Create(L"Select a DNT column above to edit its raw binary cell value:", WS_CHILD | SS_ENDELLIPSIS | SS_NOPREFIX, z, this);
    regTab(4, m_lblRawSelectedCol);
    m_edtRawCellValue.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_RAW_VALUE_EDIT); regTab(4, m_edtRawCellValue);
    m_btnRawWriteCell.Create(L"Write Raw Cell", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_RAW_WRITE_BTN); regTab(4, m_btnRawWriteCell);
    m_lblTierBaseName.Create(L"Base Item Name:", WS_CHILD, z, this); regTab(5, m_lblTierBaseName);
    m_edtTierBaseName.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_TIER_BASENAME_EDIT); regTab(5, m_edtTierBaseName);
    m_lblTierGrowth.Create(L"Growth %/Rank:", WS_CHILD, z, this); regTab(5, m_lblTierGrowth);
    m_edtTierGrowth.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_TIER_GROWTH_EDIT);
    m_edtTierGrowth.SetWindowTextW(L"28");
    regTab(5, m_edtTierGrowth);

    constexpr const wchar_t* kTierRankNames[7] = { L"Normal", L"Magic", L"Rare", L"Epic", L"Unique", L"Legend", L"Named" };
    for (size_t r = 0; r < 7; ++r) {
        m_chkTierRanks[r].Create(kTierRankNames[r], WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this,
                                 IDC_TIER_RANK_FIRST + static_cast<UINT>(r));
        if (r >= 1 && r <= 5) m_chkTierRanks[r].SetCheck(BST_CHECKED);
        regTab(5, m_chkTierRanks[r]);
    }
    m_chkTierAutoCurve.Create(L"Use Lv/Rank Stat Curve", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_TIER_AUTOCURVE_CHK); regTab(5, m_chkTierAutoCurve);
    m_chkTierAutoEcon.Create(L"Auto GearScore & Gold", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_TIER_AUTOECON_CHK);
    m_chkTierAutoEcon.SetCheck(BST_CHECKED);
    regTab(5, m_chkTierAutoEcon);
    m_chkTierRichDesc.Create(L"Auto Rich Tooltip", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_TIER_RICHDESC_CHK); regTab(5, m_chkTierRichDesc);
    m_btnRunTierSet.Create(L"Generate Multi-Rank Tier Set", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_RUN_TIER_SET); regTab(5, m_btnRunTierSet);
    m_lblSeriesTmpl.Create(L"Series Name ({CLASS}):", WS_CHILD, z, this); regTab(5, m_lblSeriesTmpl);
    m_edtSeriesTmpl.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_SERIES_TMPL_EDIT); regTab(5, m_edtSeriesTmpl);
    for (size_t c = 0; c < 10; ++c) {
        m_chkSeriesClasses[c].Create(kBaseClassLabels[c], WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this,
                                     IDC_SERIES_CLASS_FIRST + static_cast<UINT>(c));
        m_chkSeriesClasses[c].SetCheck(BST_CHECKED);
        regTab(5, m_chkSeriesClasses[c]);
    }
    m_chkSeriesAdaptStats.Create(L"Auto-Adapt Phys/Mag Stats per Class", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_SERIES_ADAPT_CHK);
    m_chkSeriesAdaptStats.SetCheck(BST_CHECKED);
    regTab(5, m_chkSeriesAdaptStats);
    m_chkSeriesRichDesc.Create(L"Auto Rich Tooltip", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_SERIES_RICHDESC_CHK); regTab(5, m_chkSeriesRichDesc);
    m_btnRunClassSeries.Create(L"Generate Class Series", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_RUN_CLASS_SERIES); regTab(5, m_btnRunClassSeries);
    m_lblBatchTargetInfo.Create(L"Target: All currently filtered items in left browser", WS_CHILD | SS_ENDELLIPSIS | SS_NOPREFIX, z, this);
    regTab(5, m_lblBatchTargetInfo);
    m_chkBatchScaleStats.Create(L"Multiply Stats x", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_BATCH_SCALE_CHK); regTab(5, m_chkBatchScaleStats);
    m_edtBatchScaleFactor.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_BATCH_SCALE_EDIT);
    m_edtBatchScaleFactor.SetWindowTextW(L"1.25");
    regTab(5, m_edtBatchScaleFactor);
    m_chkBatchAutoCurve.Create(L"Auto-Curve Stats", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_BATCH_AUTOCURVE_CHK); regTab(5, m_chkBatchAutoCurve);
    m_chkBatchSanitize.Create(L"Compact & Fix Stats", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_BATCH_SANITIZE_CHK); regTab(5, m_chkBatchSanitize);
    m_chkBatchAutoEcon.Create(L"Recalc GS & Gold", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_BATCH_AUTOECON_CHK); regTab(5, m_chkBatchAutoEcon);

    m_chkBatchSetRank.Create(L"Set Rank:", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_BATCH_SETRANK_CHK); regTab(5, m_chkBatchSetRank);
    m_cmbBatchRank.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, z, this, IDC_BATCH_RANK_CMB);
    for (const wchar_t* r : { L"0 Normal", L"1 Magic", L"2 Rare", L"3 Epic", L"4 Unique", L"5 Heroic", L"6 Named" }) {
        m_cmbBatchRank.AddString(r);
    }
    m_cmbBatchRank.SetCurSel(3);
    regTab(5, m_cmbBatchRank);

    m_chkBatchSetLevel.Create(L"Set Lv:", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_BATCH_SETLEVEL_CHK); regTab(5, m_chkBatchSetLevel);
    m_edtBatchLevel.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, z, this, IDC_BATCH_LEVEL_EDIT);
    m_edtBatchLevel.SetWindowTextW(L"95");
    regTab(5, m_edtBatchLevel);

    m_chkBatchSetOverlap.Create(L"Set Stack:", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_BATCH_SETOVERLAP_CHK); regTab(5, m_chkBatchSetOverlap);
    m_edtBatchOverlap.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, z, this, IDC_BATCH_OVERLAP_EDIT);
    m_edtBatchOverlap.SetWindowTextW(L"999");
    regTab(5, m_edtBatchOverlap);

    m_lblBatchPrefix.Create(L"Name Prefix:", WS_CHILD, z, this); regTab(5, m_lblBatchPrefix);
    m_edtBatchPrefix.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_BATCH_PREFIX_EDIT); regTab(5, m_edtBatchPrefix);
    m_lblBatchSuffix.Create(L"Suffix:", WS_CHILD, z, this); regTab(5, m_lblBatchSuffix);
    m_edtBatchSuffix.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, z, this, IDC_BATCH_SUFFIX_EDIT); regTab(5, m_edtBatchSuffix);

    m_btnRunBatchEdit.Create(L"Execute Batch Edit on Filtered Browser Items", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_RUN_BATCH_EDIT);
    regTab(5, m_btnRunBatchEdit);
    m_btnToolAudit.Create(L"Run Full Workspace Integrity Audit", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_WORKSPACE_AUDIT); regTab(6, m_btnToolAudit);
    m_btnToolVerify.Create(L"Verify 1:1 DNT Binary Round-Trip", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_VERIFY_ROUNDTRIP); regTab(6, m_btnToolVerify);
    m_btnOpenDragonJewelStudio.Create(L"Dragon Jewel & Socket Studio (Ctrl+J)...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_TOOLS_DRAGON_JEWEL); regTab(6, m_btnOpenDragonJewelStudio);
    m_btnOpenSetItemStudio.Create(L"Set Item Bonuses & Pieces (Ctrl+Shift+S)...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_TOOLS_SET_ITEM); regTab(6, m_btnOpenSetItemStudio);
    m_btnOpenEnchantStudio.Create(L"Enchantment Curve Simulator (Ctrl+Shift+E)...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_TOOLS_ENCHANT); regTab(6, m_btnOpenEnchantStudio);
    m_btnToolsActConvert.Create(L"Convert ACT v6 -> v5...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_TOOLS_ACT_CONVERT); regTab(6, m_btnToolsActConvert);
    m_btnToolsIconStudio.Create(L"Custom Icon Studio...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_TOOLS_ICON_STUDIO); regTab(6, m_btnToolsIconStudio);
    m_btnToolClearCache.Create(L"Clear Native Caches", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_CLEAR_CACHE); regTab(6, m_btnToolClearCache);
    m_btnToolSnapshot.Create(L"Crash Snapshot (.dmp)", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_CAPTURE_SNAPSHOT); regTab(6, m_btnToolSnapshot);
    m_btnToolLogs.Create(L"Open Logs Folder", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_OPEN_LOGS); regTab(6, m_btnToolLogs);

    m_btnJsonExpSel.Create(L"Export Selected Item JSON", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_JSON_EXP_SEL); regTab(6, m_btnJsonExpSel);
    m_btnJsonExpMod.Create(L"Export Modified Items JSON", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_JSON_EXP_MOD); regTab(6, m_btnJsonExpMod);
    m_btnJsonExpFilt.Create(L"Export Filtered Browser JSON", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_JSON_EXP_FILT); regTab(6, m_btnJsonExpFilt);
    m_btnJsonImpFile.Create(L"Import Items JSON File...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_JSON_IMP_FILE); regTab(6, m_btnJsonImpFile);
    m_btnJsonImpClip.Create(L"Import JSON from Clipboard", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_JSON_IMP_CLIP); regTab(6, m_btnJsonImpClip);

    m_lstModifiedItems.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                              z, this, IDC_MODIFIED_ITEMS_LIST);
    m_lstModifiedItems.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    m_lstModifiedItems.InsertColumn(0, L"Item #ID", LVCFMT_RIGHT, DpiScale(95, dpi));
    m_lstModifiedItems.InsertColumn(1, L"Item Name (Double-click to jump)", LVCFMT_LEFT, DpiScale(280, dpi));
    m_lstModifiedItems.InsertColumn(2, L"Rank", LVCFMT_LEFT, DpiScale(90, dpi));
    m_lstModifiedItems.InsertColumn(3, L"Lv", LVCFMT_RIGHT, DpiScale(55, dpi));
    m_lstModifiedItems.InsertColumn(4, L"Source DNT", LVCFMT_LEFT, DpiScale(180, dpi));
    regTab(6, m_lstModifiedItems);

    m_btnModRefresh.Create(L"Refresh Modified List", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, IDC_BTN_MOD_REFRESH); regTab(6, m_btnModRefresh);
    m_btnModUndo.Create(L"Undo Last Action (Ctrl+Z)", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_UNDO); regTab(6, m_btnModUndo);
    m_chkAutoApply.Create(L"Auto-Apply on Switch", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, z, this, IDC_CHK_AUTO_APPLY);
    m_btnApplyItem.Create(L"Apply Item Edits (Ctrl+Enter)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_APPLY);
    m_btnUndoBottom.Create(L"Undo (Ctrl+Z)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_UNDO);
    m_btnRevertItem.Create(L"Revert", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_ITEM_REVERT);
    m_btnCommitDisk.Create(L"Commit All to Disk (Ctrl+S)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, z, this, ID_CMD_COMMIT_DISK);

    for (CWnd* w : { static_cast<CWnd*>(&m_btnHdrOpen), static_cast<CWnd*>(&m_btnHdrNew), static_cast<CWnd*>(&m_btnHdrDup),
                     static_cast<CWnd*>(&m_btnHdrUndo), static_cast<CWnd*>(&m_btnHdrSaveToFolder), static_cast<CWnd*>(&m_btnHdrCommit),
                     static_cast<CWnd*>(&m_btnHdrAudit), static_cast<CWnd*>(&m_btnHdrVerify), static_cast<CWnd*>(&m_edtSearch),
                     static_cast<CWnd*>(&m_btnClearSearch), static_cast<CWnd*>(&m_cmbTableFilter), static_cast<CWnd*>(&m_cmbGroupFilter),
                     static_cast<CWnd*>(&m_cmbRankFilter), static_cast<CWnd*>(&m_chkModifiedOnly), static_cast<CWnd*>(&m_lstItems),
                     static_cast<CWnd*>(&m_lblMatchCount), static_cast<CWnd*>(&m_btnNewItem), static_cast<CWnd*>(&m_btnDupItem),
                     static_cast<CWnd*>(&m_btnDelItem), static_cast<CWnd*>(&m_chkAutoApply), static_cast<CWnd*>(&m_btnApplyItem),
                     static_cast<CWnd*>(&m_btnUndoBottom), static_cast<CWnd*>(&m_btnRevertItem), static_cast<CWnd*>(&m_btnSaveToFolder),
                     static_cast<CWnd*>(&m_btnCommitDisk), static_cast<CWnd*>(&m_grpBrowser), static_cast<CWnd*>(&m_grpSummary),
                     static_cast<CWnd*>(&m_grpBottomActions), static_cast<CWnd*>(&m_tabCtrl),
                     static_cast<CWnd*>(&m_btnBannerPickIcon) }) {
        w->SetFont(&m_fonts.regular);
    }
    for (const auto& tabVec : m_tabControls) {
        for (CWnd* w : tabVec) {
            if (w != &m_previewWnd) w->SetFont(&m_fonts.regular);
        }
    }

    // Primary actions are marked by an accent border rather than bold text.
    m_btnHdrCommit.SetPrimary(true);
    m_btnNewItem.SetPrimary(true);
    m_btnApplyItem.SetPrimary(true);
    m_btnCommitDisk.SetPrimary(true);
    m_edtNameResolved.SetFont(&m_fonts.bold);
    m_lblMidCollisionInfo.SetFont(&m_fonts.bold);
    m_lblBatchTargetInfo.SetFont(&m_fonts.bold);
    m_lblStatHdrSlot.SetFont(&m_fonts.bold);
    m_lblStatHdrType.SetFont(&m_fonts.bold);
    m_lblStatHdrProb.SetFont(&m_fonts.bold);
    m_lblStatHdrMin.SetFont(&m_fonts.bold);
    m_lblStatHdrMax.SetFont(&m_fonts.bold);
    m_edtRawCellValue.SetFont(&m_fonts.mono);

    for (CWnd* grp : { static_cast<CWnd*>(&m_grpBrowser), static_cast<CWnd*>(&m_grpSummary),
                       static_cast<CWnd*>(&m_grpBottomActions), static_cast<CWnd*>(&m_grpBasicProps),
                       static_cast<CWnd*>(&m_grpJobs), static_cast<CWnd*>(&m_grpNameMid),
                       static_cast<CWnd*>(&m_grpColorTags), static_cast<CWnd*>(&m_grpDescMid),
                       static_cast<CWnd*>(&m_grpArchetypes), static_cast<CWnd*>(&m_grpStateRows),
                       static_cast<CWnd*>(&m_grpViewport), static_cast<CWnd*>(&m_grpVisualCloner),
                       static_cast<CWnd*>(&m_grpRawInspector), static_cast<CWnd*>(&m_grpRawEdit),
                       static_cast<CWnd*>(&m_grpTierGen), static_cast<CWnd*>(&m_grpSeriesGen),
                       static_cast<CWnd*>(&m_grpBatchEdit), static_cast<CWnd*>(&m_grpTools),
                       static_cast<CWnd*>(&m_grpJsonPacks), static_cast<CWnd*>(&m_grpModDrafts) }) {
        if (grp->GetSafeHwnd()) {
            grp->SetWindowPos(&CWnd::wndBottom, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    RebuildTabControl();
    ApplyFlatChildStyles(this);
    ShowActiveEditorTab(0);
}

void CMainFrame::UpdateVisibleTabsForItem(const Data::DNItem* item) {
    std::vector<int> newVisible;
    newVisible.push_back(0); // Properties
    newVisible.push_back(1); // Localization

    if (item && m_engine.Items().HasStats(*item)) {
        newVisible.push_back(2); // Stats (1..10)
    }

    if (item && m_engine.Items().HasModel(*item)) {
        newVisible.push_back(3); // 3D & Visuals
    }

    newVisible.push_back(4); // Raw DNT
    newVisible.push_back(5); // Automation
    newVisible.push_back(6); // Tools & Packs

    if ((item && m_engine.Items().HasCharm(*item)) || m_activeTab == 7) {
        newVisible.push_back(7); // Charm & Gacha Box Probability
    }

    std::sort(newVisible.begin(), newVisible.end());

    if (newVisible != m_visibleTabs) {
        m_visibleTabs = std::move(newVisible);
        RebuildTabControl();
    }
}

std::wstring CMainFrame::GetLogicalTabTitle(int logIdx, int displayNum) const {
    switch (logIdx) {
        case 0: return std::format(L"{}. Properties", displayNum);
        case 1: return std::format(L"{}. Localization", displayNum);
        case 2: return std::format(L"{}. Stats (1..10)", displayNum);
        case 3: return std::format(L"{}. 3D & Visuals", displayNum);
        case 4: return std::format(L"{}. Raw DNT", displayNum);
        case 5: return std::format(L"{}. Automation", displayNum);
        case 6: return std::format(L"{}. Tools & Packs", displayNum);
        case 7: return std::format(L"{}. Charm & Drop Prob", displayNum);
        default: return std::format(L"{}. Tab", displayNum);
    }
}

void CMainFrame::RebuildTabControl() {
    if (!m_tabCtrl.GetSafeHwnd()) return;

    m_tabCtrl.DeleteAllItems();
    int physSel = -1;
    for (int i = 0; i < static_cast<int>(m_visibleTabs.size()); ++i) {
        const int logIdx = m_visibleTabs[static_cast<size_t>(i)];
        const std::wstring title = GetLogicalTabTitle(logIdx, i + 1);
        m_tabCtrl.InsertItem(i, title.c_str());
        if (logIdx == m_activeTab) {
            physSel = i;
        }
    }

    if (physSel >= 0) {
        m_tabCtrl.SetCurSel(physSel);
    } else {
        ShowActiveEditorTab(m_visibleTabs.empty() ? 0 : m_visibleTabs[0]);
    }
}

void CMainFrame::ShowActiveEditorTab(int tabIdx) {
    if (tabIdx < 0 || tabIdx >= kEditorTabCount) return;

    if (m_chkAutoApply.GetSafeHwnd() && m_chkAutoApply.GetCheck() == BST_CHECKED &&
        m_selectedItemId > 0 && !m_populatingEditor) {
        Data::DNItem candidate;
        std::string err;
        if (CollectItemFromEditor(candidate, err)) {
            if (const auto* cur = m_engine.Items().GetItem(m_selectedItemId)) {
                if (!ItemsEqualForAutoApply(candidate, *cur)) {
                    m_engine.ApplyItemEdit(candidate);
                }
            }
        }
    }

    m_activeTab = tabIdx;

    if (std::find(m_visibleTabs.begin(), m_visibleTabs.end(), tabIdx) == m_visibleTabs.end()) {
        m_visibleTabs.push_back(tabIdx);
        std::sort(m_visibleTabs.begin(), m_visibleTabs.end());
        RebuildTabControl();
    } else {
        auto it = std::find(m_visibleTabs.begin(), m_visibleTabs.end(), tabIdx);
        m_tabCtrl.SetCurSel(static_cast<int>(std::distance(m_visibleTabs.begin(), it)));
    }
    if (m_tabCtrl.GetSafeHwnd()) {
        m_tabCtrl.Invalidate(FALSE);
    }

    // 1. Hide controls of all inactive tabs that are currently visible
    for (int t = 0; t < kEditorTabCount; ++t) {
        if (t != tabIdx) {
            for (CWnd* w : m_tabControls[static_cast<size_t>(t)]) {
                if (w && w->GetSafeHwnd() && w->IsWindowVisible()) {
                    w->ShowWindow(SW_HIDE);
                }
            }
        }
    }

    // 2. Position the controls of the active tab at their exact coordinates before making them visible
    CRect client;
    GetClientRect(&client);
    LayoutControls(client.Width(), client.Height());

    // 3. Show controls of the new active tab and force immediate redraw
    for (CWnd* w : m_tabControls[static_cast<size_t>(tabIdx)]) {
        if (w && w->GetSafeHwnd()) {
            w->ShowWindow(SW_SHOW);
            w->Invalidate(FALSE);
        }
    }
    if (m_tabCtrl.GetSafeHwnd()) {
        m_tabCtrl.Invalidate(FALSE);
    }

    // 4. Tab-specific data synchronization & refresh
    if (tabIdx == 1) {
        UpdateLiveResolvedPreviews();
        UpdateMidCollisionBanner();
    } else if (tabIdx == 2 && m_selectedItemId > 0) {
        if (const auto* cur = m_engine.Items().GetItem(m_selectedItemId)) {
            PopulateStatWidgetsFromItem(*cur);
        }
    } else if (tabIdx == 3 && m_selectedItemId > 0) {
        m_previewWnd.SetWorkspaceEngine(&m_engine);
        if (const auto* cur = m_engine.Items().GetItem(m_selectedItemId)) {
            Graphics::ImageBuffer iconTile;
            const bool hasIcon = m_engine.GetItemIcon(cur->iconIndex, iconTile);
            auto bundle = m_engine.LoadModelForItem(*cur);
            m_previewWnd.SetPreviewData(cur, hasIcon ? &iconTile : nullptr, std::move(bundle));

            // Auto-infer character class if currently ItemOnly
            if (m_previewWnd.GetCharacterClass() == Engine::CharacterClassId::ItemOnly) {
                const auto inferred = m_engine.InferClassForItem(*cur);
                if (inferred != Engine::CharacterClassId::ItemOnly) {
                    m_cmbCharClass.SetCurSel(static_cast<int>(inferred));
                    m_previewWnd.SetCharacterClass(inferred);

                    m_cmbCharAction.ResetContent();
                    const auto acts = m_previewWnd.GetAvailableActions();
                    for (const auto& a : acts) {
                        m_cmbCharAction.AddString(Utf8ToWide(a));
                    }
                    if (m_cmbCharAction.GetCount() > 0) m_cmbCharAction.SetCurSel(0);
                }
            }
        }
        m_previewWnd.SetAutoSpin(m_chkAutoSpin.GetCheck() == BST_CHECKED);
        m_previewWnd.Invalidate(FALSE);
    } else if (tabIdx != 3) {
        m_previewWnd.SetAutoSpin(false);
    }

    if (tabIdx == 4) {
        PopulateRawInspectorList();
    } else if (tabIdx == 6) {
        RefreshModifiedItemsTabList();
    } else if (tabIdx == 7) {
        PopulateCharmTabForCurrentItem();
    }

    // 5. Targeted redraw of the right card tab area only:
    // Completely spares the left item browser (160k virtual rows) and top header bar from redraw lag.
    const int dpi = GetWindowDpi(GetSafeHwnd());
    const int bannerH = DpiScale(72, dpi);
    CRect rcRedraw(m_rcRightCard.left, m_rcRightCard.top + bannerH,
                   m_rcRightCard.right, m_rcRightCard.bottom);
    RedrawWindow(&rcRedraw, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void CMainFrame::OnTabSelChange(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    if (pResult) *pResult = 0;
    const int physSel = m_tabCtrl.GetCurSel();
    if (physSel >= 0 && physSel < static_cast<int>(m_visibleTabs.size())) {
        ShowActiveEditorTab(m_visibleTabs[static_cast<size_t>(physSel)]);
    }
}

void CMainFrame::OnTabShortcut(UINT nID) {
    const int keyNum = static_cast<int>(nID - ID_CMD_TAB_1);
    if (keyNum >= 0 && keyNum < static_cast<int>(m_visibleTabs.size())) {
        ShowActiveEditorTab(m_visibleTabs[static_cast<size_t>(keyNum)]);
    }
}

void CMainFrame::OnGetMinMaxInfo(MINMAXINFO* lpMMI) {
    CFrameWnd::OnGetMinMaxInfo(lpMMI);
    const int dpi = GetWindowDpi(GetSafeHwnd());
    lpMMI->ptMinTrackSize.x = DpiScale(1200, dpi);
    lpMMI->ptMinTrackSize.y = DpiScale(720, dpi);
}

BOOL CMainFrame::OnEraseBkgnd(CDC* pDC) {
    CRect rc;
    GetClientRect(&rc);
    pDC->FillSolidRect(&rc, Theme::kChrome);
    return TRUE;
}

HBRUSH CMainFrame::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    HBRUSH hbr = Theme::CtlColor(pDC, pWnd, nCtlColor);
    if (pWnd == &m_edtNameResolved && m_selectedItemId > 0) {
        const int rank = std::max(0, m_cmbEditRank.GetCurSel());
        pDC->SetTextColor(RankTextColor(rank, false));
    }
    return hbr ? hbr : CFrameWnd::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
    if (m_hAccel && ::TranslateAcceleratorW(GetSafeHwnd(), m_hAccel, pMsg)) {
        return TRUE;
    }
    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->wParam == VK_RETURN) {
            HWND focus = ::GetFocus();
            if (focus == m_edtSearch.GetSafeHwnd()) {
                KillTimer(kSearchDebounceTimer);
                RunItemSearch(m_selectedItemId);
                return TRUE;
            }
            if (focus == m_edtRawCellValue.GetSafeHwnd()) {
                OnRawWriteCell();
                return TRUE;
            }
        } else if (pMsg->wParam >= '1' && pMsg->wParam <= '7' && (::GetKeyState(VK_CONTROL) & 0x8000)) {
            ShowActiveEditorTab(static_cast<int>(pMsg->wParam - '1'));
            return TRUE;
        }
    }
    return CFrameWnd::PreTranslateMessage(pMsg);
}

void CMainFrame::OnPaint() {
    CPaintDC paintDC(this);
    // Top header background and hairlines: under the command strip and above the status bar.
    if (!m_rcTopHeader.IsRectEmpty()) {
        paintDC.FillSolidRect(&m_rcTopHeader, Theme::kChrome);
        paintDC.FillSolidRect(m_rcTopHeader.left, m_rcTopHeader.bottom - 1, m_rcTopHeader.Width(), 1, Theme::kBorder);
    }
    if (m_statusBar.GetSafeHwnd() && m_statusBar.IsWindowVisible()) {
        CRect sbRc;
        m_statusBar.GetWindowRect(&sbRc);
        ScreenToClient(&sbRc);
        paintDC.FillSolidRect(sbRc.left, sbRc.top - 1, sbRc.Width(), 1, Theme::kBorder);
    }
}

void CMainFrame::OnSize(UINT nType, int cx, int cy) {
    CFrameWnd::OnSize(nType, cx, cy);
    if (nType == SIZE_MINIMIZED || !m_lstItems.GetSafeHwnd()) return;
    RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
    LayoutControls(cx, cy);
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void CMainFrame::OnItemListFocusChange(NMHDR*, LRESULT* pResult) {
    // Selection color depends on focus.
    m_lstItems.Invalidate(FALSE);
    *pResult = 0;
}

void CMainFrame::LayoutControls(int cx, int cy) {
    const int dpi = GetWindowDpi(GetSafeHwnd());
    int statusH = 0;
    if (m_statusBar.GetSafeHwnd() && m_statusBar.IsWindowVisible()) {
        CRect sbRc;
        m_statusBar.GetWindowRect(&sbRc);
        statusH = sbRc.Height();
    }

    const int hdrH = DpiScale(38, dpi);
    m_rcTopHeader = CRect(0, 0, cx, hdrH);

    // Spacing is expressed in dialog units of the UI font so it tracks font and DPI together:
    // 6 DLU outer margin, 4 DLU between related controls, 14 DLU single-line control height.
    const CSize bu = Theme::DialogBaseUnits(this, m_fonts.regular);
    const int ctlGap = Theme::DluX(4, bu);
    const int ctlVGap = Theme::DluY(4, bu);

    const int pad = Theme::DluX(6, bu);
    const int gap = Theme::DluX(6, bu);
    const int usableY = hdrH + pad;
    const int usableH = std::max(100, cy - statusH - usableY - pad);
    const int usableW = std::max(100, cx - pad * 2);

    const int leftW = std::clamp(usableW * 34 / 100, DpiScale(360, dpi), DpiScale(540, dpi));
    const int rightW = std::max(DpiScale(500, dpi), usableW - leftW - gap);

    m_rcLeftCard = CRect(pad, usableY, pad + leftW, usableY + usableH);
    m_rcRightCard = CRect(m_rcLeftCard.right + gap, usableY, pad + usableW, usableY + usableH);

    HDWP hdwp = ::BeginDeferWindowPos(250);
    auto place = [&](CWnd& w, int x, int y, int width, int height) {
        if (w.GetSafeHwnd()) {
            hdwp = ::DeferWindowPos(hdwp, w.GetSafeHwnd(), nullptr, x, y, std::max(0, width), std::max(0, height),
                                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
        }
    };

    // 1. Top command bar (Responsive layout: primary left actions, responsive right-docked audit/verify)
    const int hBtnH = DpiScale(26, dpi);
    const int hBtnY = (hdrH - hBtnH) / 2;
    int hx = pad;
    place(m_btnHdrOpen, hx, hBtnY, DpiScale(114, dpi), hBtnH); hx += DpiScale(118, dpi);
    place(m_btnHdrNew, hx, hBtnY, DpiScale(84, dpi), hBtnH); hx += DpiScale(88, dpi);
    place(m_btnHdrDup, hx, hBtnY, DpiScale(80, dpi), hBtnH); hx += DpiScale(84, dpi);
    place(m_btnHdrUndo, hx, hBtnY, DpiScale(68, dpi), hBtnH); hx += DpiScale(72, dpi);
    place(m_btnHdrSaveToFolder, hx, hBtnY, DpiScale(146, dpi), hBtnH); hx += DpiScale(150, dpi);
    place(m_btnHdrCommit, hx, hBtnY, DpiScale(114, dpi), hBtnH); hx += DpiScale(118, dpi);

    const int auditW = DpiScale(78, dpi);
    const int verifyW = DpiScale(78, dpi);
    const int rightGroupW = auditW + verifyW + ctlGap;
    int rightGx = cx - pad - rightGroupW;
    if (rightGx > hx + ctlGap) {
        place(m_btnHdrAudit, rightGx, hBtnY, auditW, hBtnH);
        place(m_btnHdrVerify, rightGx + auditW + ctlGap, hBtnY, verifyW, hBtnH);
    } else {
        place(m_btnHdrAudit, hx, hBtnY, auditW, hBtnH); hx += auditW + ctlGap;
        place(m_btnHdrVerify, hx, hBtnY, verifyW, hBtnH);
    }

    // 2. Left pane (Item Browser)
    place(m_grpBrowser, m_rcLeftCard.left, m_rcLeftCard.top, m_rcLeftCard.Width(), m_rcLeftCard.Height());
    const int lx = m_rcLeftCard.left + DpiScale(12, dpi);
    const int lw = m_rcLeftCard.Width() - DpiScale(24, dpi);
    int ly = m_rcLeftCard.top + DpiScale(24, dpi);
    const int rowH = Theme::DluY(14, bu);
    const int dropH = DpiScale(320, dpi);
    const int btnH = rowH;

    const int clrW = DpiScale(50, dpi);
    place(m_edtSearch, lx, ly, lw - clrW - ctlGap, rowH);
    place(m_btnClearSearch, lx + lw - clrW, ly, clrW, rowH);
    ly += rowH + ctlGap;

    place(m_cmbTableFilter, lx, ly, lw, dropH);
    ly += rowH + ctlGap;

    const int thirdW = (lw - DpiScale(8, dpi) * 2) / 3;
    place(m_cmbGroupFilter, lx, ly, thirdW, dropH);
    place(m_cmbRankFilter, lx + thirdW + ctlGap, ly, thirdW + ctlGap, dropH);
    place(m_chkModifiedOnly, lx + (thirdW + ctlGap) * 2, ly + DpiScale(2, dpi), lw - (thirdW + ctlGap) * 2, rowH);
    ly += rowH + ctlGap;

    const int lBtmY = m_rcLeftCard.bottom - DpiScale(10, dpi) - btnH;
    const int matchLblY = lBtmY - DpiScale(18, dpi);
    const int listH = std::max(60, matchLblY - ly - DpiScale(4, dpi));
    place(m_lstItems, lx, ly, lw, listH);
    place(m_lblMatchCount, lx, matchLblY, lw, DpiScale(16, dpi));

    const int actW3 = (lw - ctlGap * 2) / 3;
    place(m_btnNewItem, lx, lBtmY, actW3, btnH);
    place(m_btnDupItem, lx + actW3 + ctlGap, lBtmY, actW3, btnH);
    place(m_btnDelItem, lx + (actW3 + ctlGap) * 2, lBtmY, lw - (actW3 + ctlGap) * 2, btnH);

    // Dynamic auto-fit for List View Column 1 (Item Name) so it fills remaining width without scrollbars
    if (m_lstItems.GetSafeHwnd()) {
        const int fixedCols = DpiScale(82 + 64 + 40 + 92 + 135, dpi); // Cols 0, 2, 3, 4, 5
        const int vScrollW = ::GetSystemMetrics(SM_CXVSCROLL) + DpiScale(4, dpi);
        const int col1W = std::max(DpiScale(120, dpi), lw - fixedCols - vScrollW);
        m_lstItems.SetColumnWidth(1, col1W);
    }

    // 3. Right pane top (Item Summary)
    const int rx = m_rcRightCard.left;
    const int rw = m_rcRightCard.Width();
    const int bannerH = DpiScale(72, dpi);
    place(m_grpSummary, rx, m_rcRightCard.top, rw, bannerH);

    const int iconSz = DpiScale(50, dpi);
    const int iconY = m_rcRightCard.top + DpiScale(14, dpi);
    place(m_bannerIconWnd, rx + DpiScale(12, dpi), iconY, iconSz, iconSz);

    const int metaX = rx + DpiScale(12, dpi) + iconSz + DpiScale(10, dpi);
    const int pickBtnW = DpiScale(96, dpi);
    const int metaW = std::max(80, rw - (metaX - rx) - pickBtnW - DpiScale(20, dpi));

    place(m_lblBannerName, metaX, iconY, metaW, DpiScale(18, dpi));
    place(m_lblBannerMeta, metaX, iconY + DpiScale(18, dpi), metaW, DpiScale(16, dpi));
    place(m_lblBannerMid, metaX, iconY + DpiScale(34, dpi), metaW, DpiScale(16, dpi));
    place(m_btnBannerPickIcon, rx + rw - pickBtnW - DpiScale(12, dpi), iconY + DpiScale(12, dpi), pickBtnW, DpiScale(26, dpi));

    // 4. Right pane middle (Tab Control - Tab Strip only so it never overlaps content)
    const int tabBarY = m_rcRightCard.top + bannerH + ctlGap;
    const int btmBarH = DpiScale(48, dpi);
    const int btmBarY = m_rcRightCard.bottom - btmBarH;
    const int tabStripH = DpiScale(26, dpi);
    place(m_tabCtrl, rx, tabBarY, rw, tabStripH);

    // 5. Right pane bottom (Bottom Actions - collision-proof responsive layout)
    place(m_grpBottomActions, rx, btmBarY, rw, btmBarH);
    const int by = btmBarY + DpiScale(14, dpi);

    const int commitW = DpiScale(106, dpi);
    const int saveW = DpiScale(148, dpi);
    const int commitX = rx + rw - commitW - DpiScale(12, dpi);
    const int saveX = commitX - saveW - ctlGap;
    place(m_btnCommitDisk, commitX, by, commitW, btnH);
    place(m_btnSaveToFolder, saveX, by, saveW, btnH);

    const int availLeftW = saveX - (rx + DpiScale(12, dpi)) - ctlGap;
    int curBx = rx + DpiScale(12, dpi);

    const int autoApplyW = std::clamp(availLeftW * 26 / 100, DpiScale(100, dpi), DpiScale(140, dpi));
    place(m_chkAutoApply, curBx, by + DpiScale(2, dpi), autoApplyW, DpiScale(22, dpi));
    curBx += autoApplyW + ctlGap;

    const int applyW = std::clamp(availLeftW * 24 / 100, DpiScale(84, dpi), DpiScale(125, dpi));
    place(m_btnApplyItem, curBx, by, applyW, btnH);
    curBx += applyW + ctlGap;

    const int revW = std::clamp(availLeftW * 14 / 100, DpiScale(58, dpi), DpiScale(76, dpi));
    place(m_btnRevertItem, curBx, by, revW, btnH);
    curBx += revW + ctlGap;

    const int undoW = std::min(DpiScale(84, dpi), saveX - curBx - ctlGap);
    if (undoW > DpiScale(36, dpi)) {
        place(m_btnUndoBottom, curBx, by, undoW, btnH);
    }

    // Tab content area positioned below the tab strip without overlapping
    const int contentX = rx + DpiScale(8, dpi);
    const int contentY = tabBarY + tabStripH + ctlGap;
    const int contentW = rw - DpiScale(16, dpi);
    const int contentH = std::max(60, btmBarY - contentY - ctlGap);

    switch (m_activeTab) {
        case 0: { // Overview & Properties (Compact responsive layout: zero vertical overflow at 720p)
            const int cardGap = DpiScale(10, dpi);
            const int cardW = (contentW - cardGap) / 2;
            CRect rcCard0(contentX, contentY, contentX + cardW, contentY + contentH);
            CRect rcCard1(contentX + cardW + cardGap, contentY, contentX + contentW, contentY + contentH);

            place(m_grpBasicProps, rcCard0.left, rcCard0.top, rcCard0.Width(), rcCard0.Height());
            place(m_grpJobs, rcCard1.left, rcCard1.top, rcCard1.Width(), rcCard1.Height());

            const int c0x = rcCard0.left + DpiScale(14, dpi);
            const int c0w = rcCard0.Width() - DpiScale(28, dpi);
            int cy0 = rcCard0.top + DpiScale(22, dpi);
            const int lW = DpiScale(84, dpi);
            const int sRowH = Theme::DluY(13, bu);
            const int vGap = DpiScale(3, dpi);

            // Row 1: Rank
            place(m_lblRank, c0x, cy0 + DpiScale(2, dpi), lW, sRowH);
            place(m_cmbEditRank, c0x + lW, cy0, c0w - lW, dropH);
            cy0 += sRowH + vGap;

            // Row 2: Icon Index + Pick Icon button
            place(m_lblIconIdx, c0x, cy0 + DpiScale(2, dpi), lW, sRowH);
            const int iconBtnsTotalW = DpiScale(170, dpi);
            const int singleBtnW = (iconBtnsTotalW - ctlGap) / 2;
            place(m_edtIconIndex, c0x + lW, cy0, c0w - lW - iconBtnsTotalW - ctlGap, sRowH);
            place(m_btnBrowseIcon, c0x + c0w - iconBtnsTotalW, cy0, singleBtnW, sRowH);
            place(m_btnIconEditor, c0x + c0w - singleBtnW, cy0, singleBtnW, sRowH);
            cy0 += sRowH + vGap;

            // Row 3: Level Limit & Hero Level side-by-side
            const int halfRowW = (c0w - ctlGap) / 2;
            const int subLblW = DpiScale(68, dpi);
            place(m_lblLevel, c0x, cy0 + DpiScale(2, dpi), subLblW, sRowH);
            place(m_edtLevel, c0x + subLblW, cy0, halfRowW - subLblW, sRowH);

            const int hLvlX = c0x + halfRowW + ctlGap;
            place(m_lblHeroLevel, hLvlX, cy0 + DpiScale(2, dpi), subLblW, sRowH);
            place(m_edtHeroLevel, hLvlX + subLblW, cy0, c0w - (hLvlX - c0x) - subLblW, sRowH);
            cy0 += sRowH + vGap;

            // Row 4: Max Stack & Durability side-by-side
            place(m_lblOverlap, c0x, cy0 + DpiScale(2, dpi), subLblW, sRowH);
            place(m_edtOverlap, c0x + subLblW, cy0, halfRowW - subLblW, sRowH);

            const int duraAutoW = DpiScale(44, dpi);
            place(m_lblDurability, hLvlX, cy0 + DpiScale(2, dpi), subLblW, sRowH);
            place(m_edtDurability, hLvlX + subLblW, cy0, c0w - (hLvlX - c0x) - subLblW - duraAutoW - ctlGap, sRowH);
            place(m_btnAutoDurability, c0x + c0w - duraAutoW, cy0, duraAutoW, sRowH);
            cy0 += sRowH + vGap;

            // Row 5: Sell Copper
            const int autoW = DpiScale(44, dpi);
            place(m_lblSell, c0x, cy0 + DpiScale(2, dpi), lW, sRowH);
            place(m_edtSell, c0x + lW, cy0, c0w - lW - autoW - ctlGap, sRowH);
            place(m_btnAutoSell, c0x + c0w - autoW, cy0, autoW, sRowH);
            cy0 += sRowH + vGap;

            // Row 6: Gear Score
            place(m_lblGearScore, c0x, cy0 + DpiScale(2, dpi), lW, sRowH);
            place(m_edtGearScore, c0x + lW, cy0, c0w - lW - autoW - ctlGap, sRowH);
            place(m_btnAutoGearScore, c0x + c0w - autoW, cy0, autoW, sRowH);
            cy0 += sRowH + DpiScale(5, dpi);

            // Row 7: Auto-Calculate All Economy
            place(m_btnAutoAllEconomy, c0x, cy0, c0w, sRowH);
            cy0 += sRowH + DpiScale(6, dpi);

            // Row 8: Storage & Cash Flags
            const int flagW = (c0w - DpiScale(8, dpi) * 2) / 3;
            place(m_chkCash, c0x, cy0, flagW, sRowH);
            place(m_chkStorage, c0x + flagW + ctlGap, cy0, flagW, sRowH);
            place(m_chkWStorage, c0x + (flagW + ctlGap) * 2, cy0, c0w - (flagW + ctlGap) * 2, sRowH);
            cy0 += sRowH + DpiScale(6, dpi);

            // Row 9: Presets
            const int prsW = (c0w - DpiScale(8, dpi) * 2) / 3;
            place(m_btnPresetEquip, c0x, cy0, prsW, sRowH);
            place(m_btnPresetStack, c0x + prsW + ctlGap, cy0, prsW, sRowH);
            place(m_btnPresetCash, c0x + (prsW + ctlGap) * 2, cy0, c0w - (prsW + ctlGap) * 2, sRowH);
            cy0 += sRowH + DpiScale(6, dpi);

            // Row 10: Dragon Jewel Studio
            const int linkBtnW = DpiScale(140, dpi);
            const int linkLblW = c0w - linkBtnW - ctlGap;
            place(m_lblPropJewelInfo, c0x, cy0 + DpiScale(2, dpi), linkLblW, sRowH);
            place(m_btnPropJewelStudio, c0x + linkLblW + ctlGap, cy0, linkBtnW, sRowH);
            cy0 += sRowH + vGap;

            // Row 11: Set Item Studio
            place(m_lblPropSetItemInfo, c0x, cy0 + DpiScale(2, dpi), linkLblW, sRowH);
            place(m_btnPropSetItemStudio, c0x + linkLblW + ctlGap, cy0, linkBtnW, sRowH);
            cy0 += sRowH + vGap;

            // Row 12: Enchant Studio
            place(m_lblPropEnchantInfo, c0x, cy0 + DpiScale(2, dpi), linkLblW, sRowH);
            place(m_btnPropEnchantStudio, c0x + linkLblW + ctlGap, cy0, linkBtnW, sRowH);

            // Right Card: Job Matrix
            const int c1x = rcCard1.left + DpiScale(14, dpi);
            const int c1w = rcCard1.Width() - DpiScale(28, dpi);
            int cy1 = rcCard1.top + DpiScale(22, dpi);

            place(m_lblJobs, c1x, cy1 + DpiScale(2, dpi), DpiScale(120, dpi), sRowH);
            place(m_edtJobs, c1x + DpiScale(124, dpi), cy1, c1w - DpiScale(124, dpi), sRowH);
            cy1 += sRowH + DpiScale(6, dpi);

            place(m_chkClassAll, c1x, cy1, c1w, sRowH);
            cy1 += sRowH + DpiScale(4, dpi);

            const int colW = (c1w - DpiScale(10, dpi)) / 2;
            for (size_t i = 0; i < m_chkBaseClasses.size(); ++i) {
                const int col = static_cast<int>(i % 2);
                const int row = static_cast<int>(i / 2);
                const int x = c1x + col * (colW + DpiScale(10, dpi));
                const int y = cy1 + row * (sRowH + DpiScale(2, dpi));
                place(m_chkBaseClasses[i], x, y, colW, sRowH);
            }
            cy1 += 5 * (sRowH + DpiScale(2, dpi)) + DpiScale(6, dpi);

            // Subclasses header row & action buttons
            const int btnW4 = std::clamp((c1w - DpiScale(160, dpi) - DpiScale(12, dpi)) / 4, DpiScale(46, dpi), DpiScale(68, dpi));
            const int lblW_sub = c1w - (btnW4 * 4 + DpiScale(12, dpi));
            int curSubX = c1x;
            place(m_lblSubclasses, curSubX, cy1 + DpiScale(2, dpi), lblW_sub, sRowH);
            curSubX += lblW_sub + DpiScale(4, dpi);
            place(m_btnSubclassAll1st, curSubX, cy1, btnW4, sRowH);
            curSubX += btnW4 + DpiScale(4, dpi);
            place(m_btnSubclassAll2nd, curSubX, cy1, btnW4, sRowH);
            curSubX += btnW4 + DpiScale(4, dpi);
            place(m_btnSubclassClear, curSubX, cy1, btnW4, sRowH);
            curSubX += btnW4 + DpiScale(4, dpi);
            place(m_btnSubclassExpand, curSubX, cy1, btnW4, sRowH);
            cy1 += sRowH + DpiScale(6, dpi);

            // Bottom resolved label
            const int resolvedH = DpiScale(38, dpi);
            const int resolvedY = rcCard1.bottom - DpiScale(10, dpi) - resolvedH;
            const int treeH = std::max<int>(DpiScale(90, dpi), resolvedY - cy1 - DpiScale(6, dpi));

            place(m_treeSubclasses, c1x, cy1, c1w, treeH);
            place(m_lblJobsResolved, c1x, resolvedY, c1w, resolvedH);
            break;
        }
        case 1: { // Localization
            CRect rcLocCard(contentX, contentY, contentX + contentW, contentY + contentH);

            const int rowStep = rowH + ctlVGap;
            const int nameGrpH = DpiScale(24, dpi) + 3 * rowStep;
            const int colorGrpY = nameGrpH + ctlVGap;
            const int colorGrpH = rowH + DpiScale(20, dpi);
            const int descGrpY = colorGrpY + colorGrpH + ctlVGap;
            place(m_grpNameMid, rcLocCard.left, rcLocCard.top, rcLocCard.Width(), nameGrpH);
            place(m_grpColorTags, rcLocCard.left, rcLocCard.top + colorGrpY, rcLocCard.Width(), colorGrpH);
            place(m_grpDescMid, rcLocCard.left, rcLocCard.top + descGrpY, rcLocCard.Width(), rcLocCard.Height() - descGrpY);

            const int lx0 = rcLocCard.left + DpiScale(14, dpi);
            const int lw0 = rcLocCard.Width() - DpiScale(28, dpi);
            int ly0 = rcLocCard.top + DpiScale(24, dpi);
            const int lW = DpiScale(130, dpi);
            const int midW = DpiScale(110, dpi);
            const int allocW = DpiScale(120, dpi);
            const int searchW = DpiScale(130, dpi);

            place(m_lblNameMid, lx0, ly0 + DpiScale(3, dpi), lW, rowH);
            place(m_edtNameMid, lx0 + lW, ly0, midW, rowH);
            place(m_btnAllocNameMid, lx0 + lW + midW + ctlGap, ly0, allocW, rowH);
            place(m_btnSearchNameMid, lx0 + lW + midW + allocW + DpiScale(12, dpi), ly0, searchW, rowH);

            const int pLeft = lx0 + lW + midW + allocW + searchW + DpiScale(18, dpi);
            place(m_lblNameParam, pLeft, ly0 + DpiScale(3, dpi), DpiScale(52, dpi), rowH);
            place(m_edtNameParam, pLeft + DpiScale(54, dpi), ly0, std::max(40, lx0 + lw0 - (pLeft + DpiScale(54, dpi))), rowH);
            ly0 += rowH + ctlGap;

            place(m_lblNameTmpl, lx0, ly0 + DpiScale(3, dpi), lW, rowH);
            place(m_edtNameTmpl, lx0 + lW, ly0, lw0 - lW, rowH);
            ly0 += rowH + ctlGap;

            place(m_lblNameResolvedHdr, lx0, ly0 + DpiScale(3, dpi), lW, rowH);
            place(m_edtNameResolved, lx0 + lW, ly0, lw0 - lW, rowH);

            // Color tag bar inside m_grpColorTags
            int colY = rcLocCard.top + colorGrpY + DpiScale(14, dpi);
            const int tagW = (lw0 - ctlGap * 6) / 7;
            for (size_t k = 0; k < 7; ++k) {
                place(m_btnColorTags[k], lx0 + static_cast<int>(k) * (tagW + ctlGap), colY, tagW, rowH);
            }

            ly0 = rcLocCard.top + descGrpY + DpiScale(20, dpi);
            place(m_lblDescMid, lx0, ly0 + DpiScale(3, dpi), lW, rowH);
            place(m_edtDescMid, lx0 + lW, ly0, midW, rowH);
            place(m_btnAllocDescMid, lx0 + lW + midW + ctlGap, ly0, allocW, rowH);
            place(m_btnSearchDescMid, lx0 + lW + midW + allocW + DpiScale(12, dpi), ly0, searchW, rowH);

            place(m_lblDescParam, pLeft, ly0 + DpiScale(3, dpi), DpiScale(52, dpi), rowH);
            place(m_edtDescParam, pLeft + DpiScale(54, dpi), ly0, std::max(40, lx0 + lw0 - (pLeft + DpiScale(54, dpi))), rowH);
            ly0 += rowH + ctlGap;

            const int remH = std::max<int>(DpiScale(90, dpi), static_cast<int>(rcLocCard.bottom - DpiScale(12, dpi) - ly0));
            const int tmplH = (remH - ctlGap) * 52 / 100;
            const int liveH = remH - tmplH - ctlGap;

            place(m_lblDescTmpl, lx0, ly0 + DpiScale(3, dpi), lW, rowH);
            place(m_edtDescTmpl, lx0 + lW, ly0, lw0 - lW, tmplH);
            ly0 += tmplH + ctlGap;

            place(m_lblDescResolvedHdr, lx0, ly0 + DpiScale(3, dpi), lW, rowH);
            place(m_edtDescResolved, lx0 + lW, ly0, lw0 - lW, liveH);
            break;
        }
        case 2: { // Stats & Potentials (1..10) - Responsive button wrap and proportional columns
            CRect rcStatCard(contentX, contentY, contentX + contentW, contentY + contentH);

            const int archGrpH = DpiScale(78, dpi);
            place(m_grpArchetypes, rcStatCard.left, rcStatCard.top, rcStatCard.Width(), archGrpH);
            place(m_grpStateRows, rcStatCard.left, rcStatCard.top + archGrpH + DpiScale(4, dpi), rcStatCard.Width(), rcStatCard.Height() - archGrpH - DpiScale(4, dpi));

            const int sx = rcStatCard.left + DpiScale(14, dpi);
            const int sw = rcStatCard.Width() - DpiScale(28, dpi);
            int sy = rcStatCard.top + DpiScale(20, dpi);

            // Row 1: Archetypes & Curves
            place(m_lblArchetype, sx, sy + DpiScale(3, dpi), DpiScale(84, dpi), rowH);
            const int archW = std::clamp(sw * 34 / 100, DpiScale(220, dpi), DpiScale(320, dpi));
            place(m_cmbArchetype, sx + DpiScale(88, dpi), sy, archW, dropH);
            place(m_btnApplyArchetype, sx + DpiScale(88, dpi) + archW + ctlGap, sy, DpiScale(130, dpi), rowH);
            place(m_btnStatsAutoCurve, sx + DpiScale(88, dpi) + archW + ctlGap + DpiScale(136, dpi), sy, DpiScale(160, dpi), rowH);
            sy += rowH + DpiScale(4, dpi);

            // Row 2: Multipliers and helper buttons (Responsive wrap)
            const int scaleBtnW = DpiScale(58, dpi);
            int scX = sx;
            for (size_t s = 0; s < 5; ++s) {
                place(m_btnStatScales[s], scX, sy, scaleBtnW, DpiScale(22, dpi));
                scX += scaleBtnW + ctlGap;
            }
            place(m_btnStatsSpread15, scX, sy, DpiScale(100, dpi), DpiScale(22, dpi)); scX += DpiScale(106, dpi);
            place(m_btnStatsMinToMax, scX, sy, DpiScale(80, dpi), DpiScale(22, dpi)); scX += DpiScale(86, dpi);
            place(m_btnStatsProbsFull, scX, sy, DpiScale(80, dpi), DpiScale(22, dpi)); scX += DpiScale(86, dpi);
            place(m_btnStatsCompact, scX, sy, DpiScale(72, dpi), DpiScale(22, dpi)); scX += DpiScale(78, dpi);
            place(m_btnStatsCopy, scX, sy, DpiScale(72, dpi), DpiScale(22, dpi)); scX += DpiScale(78, dpi);
            place(m_btnStatsPaste, scX, sy, DpiScale(70, dpi), DpiScale(22, dpi)); scX += DpiScale(76, dpi);
            place(m_btnStatsClear, scX, sy, DpiScale(64, dpi), DpiScale(22, dpi));

            sy = rcStatCard.top + archGrpH + DpiScale(22, dpi);

            const int slotW = DpiScale(32, dpi);
            const int probW = DpiScale(70, dpi);
            const int sGap = DpiScale(8, dpi);
            const int typeW = std::clamp((sw - slotW - probW - sGap * 5) * 42 / 100, DpiScale(200, dpi), DpiScale(340, dpi));
            const int valW = std::max(DpiScale(90, dpi), (sw - slotW - typeW - probW - sGap * 5) / 2);

            const int sx0 = sx;
            const int sx1 = sx0 + slotW + sGap;
            const int sx2 = sx1 + typeW + sGap;
            const int sx3 = sx2 + probW + sGap;
            const int sx4 = sx3 + valW + sGap;

            place(m_lblStatHdrSlot, sx0, sy, slotW, DpiScale(18, dpi));
            place(m_lblStatHdrType, sx1, sy, typeW, DpiScale(18, dpi));
            place(m_lblStatHdrProb, sx2, sy, probW, DpiScale(18, dpi));
            place(m_lblStatHdrMin, sx3, sy, valW, DpiScale(18, dpi));
            place(m_lblStatHdrMax, sx4, sy, valW, DpiScale(18, dpi));
            sy += DpiScale(20, dpi);

            const int statRowH = DpiScale(23, dpi);
            for (size_t i = 0; i < Data::kStateSlotCount; ++i) {
                auto& rw = m_statWidgets[i];
                place(rw.lblSlot, sx0, sy + DpiScale(2, dpi), slotW, statRowH);
                place(rw.cmbType, sx1, sy, typeW, dropH);
                place(rw.edtProb, sx2, sy, probW, statRowH);
                place(rw.edtMin, sx3, sy, valW, statRowH);
                place(rw.edtMax, sx4, sy, valW, statRowH);
                sy += statRowH + DpiScale(3, dpi);
            }
            break;
        }
        case 3: { // 3D & Visuals - Responsive top bar and donor cloner
            CRect rc3dCard(contentX, contentY, contentX + contentW, contentY + contentH);

            const int clonerH = DpiScale(44, dpi);
            place(m_grpViewport, rc3dCard.left, rc3dCard.top, rc3dCard.Width(), rc3dCard.Height() - clonerH - ctlGap);
            place(m_grpVisualCloner, rc3dCard.left, rc3dCard.bottom - clonerH, rc3dCard.Width(), clonerH);

            const int mx = rc3dCard.left + DpiScale(14, dpi);
            const int mw = rc3dCard.Width() - DpiScale(28, dpi);
            int my = rc3dCard.top + DpiScale(22, dpi);

            place(m_lblRenderMode, mx, my + DpiScale(3, dpi), DpiScale(62, dpi), rowH);
            const int modeW = std::clamp(mw * 28 / 100, DpiScale(180, dpi), DpiScale(260, dpi));
            place(m_cmbRenderMode, mx + DpiScale(66, dpi), my, modeW, dropH);

            int cx3 = mx + DpiScale(66, dpi) + modeW + DpiScale(8, dpi);
            const int spinW = DpiScale(110, dpi);
            place(m_chkAutoSpin, cx3, my + DpiScale(2, dpi), spinW, DpiScale(22, dpi)); cx3 += spinW + ctlGap;

            const int zoomW = DpiScale(58, dpi);
            const int resetW = DpiScale(86, dpi);
            const int atlasW = DpiScale(130, dpi);
            place(m_btnZoomIn3D, cx3, my, zoomW, rowH); cx3 += zoomW + ctlGap;
            place(m_btnZoomOut3D, cx3, my, zoomW, rowH); cx3 += zoomW + ctlGap;
            place(m_btnResetCam, cx3, my, resetW, rowH); cx3 += resetW + ctlGap;
            const int actV5BtnW = DpiScale(130, dpi);
            place(m_btnOpenIconAtlas, cx3, my, atlasW, rowH); cx3 += atlasW + ctlGap;
            if (cx3 + actV5BtnW <= mx + mw) {
                place(m_btnViewportActConvert, cx3, my, actV5BtnW, rowH);
            } else {
                place(m_btnViewportActConvert, mx + mw - actV5BtnW, my, actV5BtnW, rowH);
            }
            my += rowH + DpiScale(6, dpi);

            // Row 2: ACT Viewer Toolbar (Class, Action, Play/Pause, Reset, Loop, Speed)
            int rx2 = mx;
            place(m_lblCharClass, rx2, my + DpiScale(3, dpi), DpiScale(92, dpi), rowH); rx2 += DpiScale(96, dpi);
            const int classCmbW = DpiScale(125, dpi);
            place(m_cmbCharClass, rx2, my, classCmbW, dropH); rx2 += classCmbW + ctlGap;

            place(m_lblCharAction, rx2, my + DpiScale(3, dpi), DpiScale(44, dpi), rowH); rx2 += DpiScale(48, dpi);
            const int actCmbW = DpiScale(150, dpi);
            place(m_cmbCharAction, rx2, my, actCmbW, dropH); rx2 += actCmbW + ctlGap;

            const int ppBtnW = DpiScale(68, dpi);
            place(m_btnAnimPlayPause, rx2, my, ppBtnW, btnH); rx2 += ppBtnW + ctlGap;
            const int rstBtnW = DpiScale(62, dpi);
            place(m_btnAnimReset, rx2, my, rstBtnW, btnH); rx2 += rstBtnW + ctlGap;

            const int loopW = DpiScale(54, dpi);
            place(m_chkAnimLoop, rx2, my + DpiScale(2, dpi), loopW, DpiScale(22, dpi)); rx2 += loopW + ctlGap;

            place(m_lblAnimSpeed, rx2, my + DpiScale(3, dpi), DpiScale(42, dpi), rowH); rx2 += DpiScale(46, dpi);
            const int spdCmbW = DpiScale(68, dpi);
            place(m_cmbAnimSpeed, rx2, my, spdCmbW, dropH);
            my += rowH + DpiScale(8, dpi);

            const int clonerY = rc3dCard.bottom - clonerH + DpiScale(14, dpi);
            place(m_previewWnd, mx, my, mw, std::max<int>(60, static_cast<int>(rc3dCard.bottom - clonerH - DpiScale(10, dpi) - my)));

            place(m_lblDonorId, mx, clonerY + DpiScale(3, dpi), DpiScale(100, dpi), rowH);
            const int donEdtW = DpiScale(90, dpi);
            place(m_edtDonorId, mx + DpiScale(104, dpi), clonerY, donEdtW, rowH);

            const int remClonerW = mw - DpiScale(104, dpi) - donEdtW - ctlGap;
            const int clBtnW = std::clamp(remClonerW / 4 - ctlGap, DpiScale(90, dpi), DpiScale(140, dpi));
            int clX = mx + DpiScale(104, dpi) + donEdtW + ctlGap;
            place(m_btnUseSelAsDonor, clX, clonerY, clBtnW, btnH); clX += clBtnW + ctlGap;
            place(m_btnCloneVisualBoth, clX, clonerY, clBtnW, btnH); clX += clBtnW + ctlGap;
            place(m_btnCloneSkinOnly, clX, clonerY, clBtnW, btnH); clX += clBtnW + ctlGap;
            place(m_btnCloneIconOnly, clX, clonerY, mw - (clX - mx), btnH);
            break;
        }
        case 4: { // Raw DNT Inspector
            CRect rcRawCard(contentX, contentY, contentX + contentW, contentY + contentH);

            const int rEditH = DpiScale(44, dpi);
            place(m_grpRawInspector, rcRawCard.left, rcRawCard.top, rcRawCard.Width(), rcRawCard.Height() - rEditH - ctlGap);
            place(m_grpRawEdit, rcRawCard.left, rcRawCard.bottom - rEditH, rcRawCard.Width(), rEditH);

            const int rx0 = rcRawCard.left + DpiScale(14, dpi);
            const int rw0 = rcRawCard.Width() - DpiScale(28, dpi);
            int ry0 = rcRawCard.top + DpiScale(22, dpi);

            place(m_lblRawSource, rx0, ry0 + DpiScale(3, dpi), DpiScale(48, dpi), rowH);
            const int srcW = DpiScale(280, dpi);
            place(m_cmbRawSource, rx0 + DpiScale(52, dpi), ry0, srcW, dropH);
            const int fltW = DpiScale(220, dpi);
            place(m_edtRawFilter, rx0 + DpiScale(52, dpi) + srcW + DpiScale(10, dpi), ry0, fltW, rowH);

            int actRx = rx0 + rw0;
            actRx -= DpiScale(190, dpi);
            place(m_btnRawCloneDetail, actRx, ry0, DpiScale(190, dpi), rowH);
            actRx -= DpiScale(8, dpi) + DpiScale(120, dpi);
            place(m_btnRawCopyTsv, actRx, ry0, DpiScale(120, dpi), rowH);
            ry0 += rowH + ctlGap;

            const int rEditY = rcRawCard.bottom - rEditH + DpiScale(14, dpi);
            const int rSelLblY = rcRawCard.bottom - rEditH - DpiScale(26, dpi);

            place(m_lstRawCols, rx0, ry0, rw0, std::max(60, rSelLblY - ry0 - DpiScale(4, dpi)));
            place(m_lblRawSelectedCol, rx0, rSelLblY, rw0, DpiScale(18, dpi));

            const int writeW = DpiScale(120, dpi);
            place(m_edtRawCellValue, rx0, rEditY, rw0 - writeW - DpiScale(8, dpi), rowH);
            place(m_btnRawWriteCell, rx0 + rw0 - writeW, rEditY, writeW, rowH);
            break;
        }
        case 5: { // Automation & Batch Studio
            const int cardGap = DpiScale(10, dpi);
            const int cardW = (contentW - cardGap * 2) / 3;
            CRect rcCardA(contentX, contentY, contentX + cardW, contentY + contentH);
            CRect rcCardB(contentX + cardW + cardGap, contentY, contentX + cardW * 2 + cardGap, contentY + contentH);
            CRect rcCardC(contentX + cardW * 2 + cardGap * 2, contentY, contentX + contentW, contentY + contentH);

            place(m_grpTierGen, rcCardA.left, rcCardA.top, rcCardA.Width(), rcCardA.Height());
            place(m_grpSeriesGen, rcCardB.left, rcCardB.top, rcCardB.Width(), rcCardB.Height());
            place(m_grpBatchEdit, rcCardC.left, rcCardC.top, rcCardC.Width(), rcCardC.Height());

            const int ax = rcCardA.left + DpiScale(12, dpi);
            const int aw = rcCardA.Width() - DpiScale(24, dpi);
            int ay = rcCardA.top + DpiScale(24, dpi);

            place(m_lblTierBaseName, ax, ay, aw, DpiScale(18, dpi)); ay += DpiScale(20, dpi);
            place(m_edtTierBaseName, ax, ay, aw, rowH); ay += rowH + DpiScale(8, dpi);

            place(m_lblTierGrowth, ax, ay + DpiScale(3, dpi), DpiScale(110, dpi), rowH);
            place(m_edtTierGrowth, ax + DpiScale(114, dpi), ay, aw - DpiScale(114, dpi), rowH); ay += rowH + DpiScale(8, dpi);

            for (size_t r = 0; r < 7; ++r) {
                place(m_chkTierRanks[r], ax, ay, aw, DpiScale(22, dpi));
                ay += DpiScale(24, dpi);
            }
            ay += ctlGap;
            place(m_chkTierAutoCurve, ax, ay, aw, DpiScale(22, dpi)); ay += DpiScale(24, dpi);
            place(m_chkTierAutoEcon, ax, ay, aw, DpiScale(22, dpi)); ay += DpiScale(24, dpi);
            place(m_chkTierRichDesc, ax, ay, aw, DpiScale(22, dpi)); ay += DpiScale(28, dpi);
            place(m_btnRunTierSet, ax, rcCardA.bottom - DpiScale(12, dpi) - btnH, aw, btnH);

            const int bx = rcCardB.left + DpiScale(12, dpi);
            const int bw = rcCardB.Width() - DpiScale(24, dpi);
            int by = rcCardB.top + DpiScale(24, dpi);

            place(m_lblSeriesTmpl, bx, by, bw, DpiScale(18, dpi)); by += DpiScale(20, dpi);
            place(m_edtSeriesTmpl, bx, by, bw, rowH); by += rowH + DpiScale(8, dpi);

            const int halfBw = (bw - DpiScale(8, dpi)) / 2;
            for (size_t c = 0; c < 10; ++c) {
                const int col = static_cast<int>(c % 2);
                const int row = static_cast<int>(c / 2);
                const int px = bx + col * (halfBw + DpiScale(8, dpi));
                const int py = by + row * DpiScale(24, dpi);
                place(m_chkSeriesClasses[c], px, py, halfBw, DpiScale(22, dpi));
            }
            by += DpiScale(128, dpi);
            place(m_chkSeriesAdaptStats, bx, by, bw, DpiScale(22, dpi)); by += DpiScale(24, dpi);
            place(m_chkSeriesRichDesc, bx, by, bw, DpiScale(22, dpi)); by += DpiScale(28, dpi);
            place(m_btnRunClassSeries, bx, rcCardB.bottom - DpiScale(12, dpi) - btnH, bw, btnH);

            const int cx0 = rcCardC.left + DpiScale(12, dpi);
            const int cw0 = rcCardC.Width() - DpiScale(24, dpi);
            int cy0 = rcCardC.top + DpiScale(24, dpi);

            place(m_lblBatchTargetInfo, cx0, cy0, cw0, DpiScale(22, dpi)); cy0 += DpiScale(26, dpi);

            place(m_chkBatchScaleStats, cx0, cy0 + DpiScale(3, dpi), DpiScale(130, dpi), rowH);
            place(m_edtBatchScaleFactor, cx0 + DpiScale(134, dpi), cy0, cw0 - DpiScale(134, dpi), rowH); cy0 += rowH + ctlGap;

            place(m_chkBatchAutoCurve, cx0, cy0, cw0, DpiScale(22, dpi)); cy0 += DpiScale(24, dpi);
            place(m_chkBatchSanitize, cx0, cy0, cw0, DpiScale(22, dpi)); cy0 += DpiScale(24, dpi);
            place(m_chkBatchAutoEcon, cx0, cy0, cw0, DpiScale(22, dpi)); cy0 += DpiScale(26, dpi);

            place(m_chkBatchSetRank, cx0, cy0 + DpiScale(3, dpi), DpiScale(90, dpi), rowH);
            place(m_cmbBatchRank, cx0 + DpiScale(94, dpi), cy0, cw0 - DpiScale(94, dpi), dropH); cy0 += rowH + ctlGap;

            place(m_chkBatchSetLevel, cx0, cy0 + DpiScale(3, dpi), DpiScale(90, dpi), rowH);
            place(m_edtBatchLevel, cx0 + DpiScale(94, dpi), cy0, cw0 - DpiScale(94, dpi), rowH); cy0 += rowH + ctlGap;

            place(m_chkBatchSetOverlap, cx0, cy0 + DpiScale(3, dpi), DpiScale(90, dpi), rowH);
            place(m_edtBatchOverlap, cx0 + DpiScale(94, dpi), cy0, cw0 - DpiScale(94, dpi), rowH); cy0 += rowH + DpiScale(8, dpi);

            place(m_lblBatchPrefix, cx0, cy0 + DpiScale(3, dpi), DpiScale(86, dpi), rowH);
            place(m_edtBatchPrefix, cx0 + DpiScale(90, dpi), cy0, cw0 - DpiScale(90, dpi), rowH); cy0 += rowH + ctlGap;

            place(m_lblBatchSuffix, cx0, cy0 + DpiScale(3, dpi), DpiScale(86, dpi), rowH);
            place(m_edtBatchSuffix, cx0 + DpiScale(90, dpi), cy0, cw0 - DpiScale(90, dpi), rowH); cy0 += rowH + DpiScale(8, dpi);

            place(m_btnRunBatchEdit, cx0, rcCardC.bottom - DpiScale(12, dpi) - btnH, cw0, btnH);
            break;
        }
        case 6: { // Tools, Audit & JSON I/O - Responsive 2-column dashboard in left card
            const int cardGap = DpiScale(10, dpi);
            const int cardW = (contentW - cardGap) / 2;
            CRect rcCardL(contentX, contentY, contentX + cardW, contentY + contentH);
            CRect rcCardR(contentX + cardW + cardGap, contentY, contentX + contentW, contentY + contentH);

            place(m_grpTools, rcCardL.left, rcCardL.top, rcCardL.Width(), rcCardL.Height());
            place(m_grpModDrafts, rcCardR.left, rcCardR.top, rcCardR.Width(), rcCardR.Height());

            const int tx0 = rcCardL.left + DpiScale(14, dpi);
            const int tw0 = rcCardL.Width() - DpiScale(28, dpi);
            int ty0 = rcCardL.top + DpiScale(24, dpi);

            // Responsive 2-column grid inside m_grpTools
            const int colW6 = (tw0 - ctlGap) / 2;
            const int col1X = tx0;
            const int col2X = tx0 + colW6 + ctlGap;

            // Column 1: Diagnostics & Studios
            int tyCol1 = ty0;
            place(m_btnToolAudit, col1X, tyCol1, colW6, btnH); tyCol1 += btnH + DpiScale(6, dpi);
            place(m_btnToolVerify, col1X, tyCol1, colW6, btnH); tyCol1 += btnH + DpiScale(6, dpi);
            place(m_btnOpenDragonJewelStudio, col1X, tyCol1, colW6, btnH); tyCol1 += btnH + DpiScale(6, dpi);
            place(m_btnOpenSetItemStudio, col1X, tyCol1, colW6, btnH); tyCol1 += btnH + DpiScale(6, dpi);
            place(m_btnOpenEnchantStudio, col1X, tyCol1, colW6, btnH); tyCol1 += btnH + DpiScale(6, dpi);
            place(m_btnToolsActConvert, col1X, tyCol1, colW6, btnH); tyCol1 += btnH + DpiScale(6, dpi);
            place(m_btnToolLogs, col1X, tyCol1, colW6, btnH);

            // Column 2: Cache, Snapshots & JSON Packs
            int tyCol2 = ty0;
            place(m_btnToolsIconStudio, col2X, tyCol2, colW6, btnH); tyCol2 += btnH + DpiScale(6, dpi);
            place(m_btnToolClearCache, col2X, tyCol2, colW6, btnH); tyCol2 += btnH + DpiScale(6, dpi);
            place(m_btnToolSnapshot, col2X, tyCol2, colW6, btnH); tyCol2 += btnH + DpiScale(6, dpi);
            place(m_btnJsonExpSel, col2X, tyCol2, colW6, btnH); tyCol2 += btnH + DpiScale(6, dpi);
            place(m_btnJsonExpMod, col2X, tyCol2, colW6, btnH); tyCol2 += btnH + DpiScale(6, dpi);
            place(m_btnJsonExpFilt, col2X, tyCol2, colW6, btnH); tyCol2 += btnH + DpiScale(6, dpi);

            const int halfColW = (colW6 - ctlGap) / 2;
            place(m_btnJsonImpFile, col2X, tyCol2, halfColW, btnH);
            place(m_btnJsonImpClip, col2X + halfColW + ctlGap, tyCol2, colW6 - halfColW - ctlGap, btnH);

            // Right Card: Modified items list
            const int mx0 = rcCardR.left + DpiScale(14, dpi);
            const int mw0 = rcCardR.Width() - DpiScale(28, dpi);
            int my0 = rcCardR.top + DpiScale(24, dpi);

            const int btmModH = btnH;
            const int btmModY = rcCardR.bottom - DpiScale(12, dpi) - btmModH;
            place(m_lstModifiedItems, mx0, my0, mw0, std::max(60, btmModY - my0 - DpiScale(8, dpi)));

            const int halfMw = (mw0 - DpiScale(8, dpi)) / 2;
            place(m_btnModRefresh, mx0, btmModY, halfMw, btmModH);
            place(m_btnModUndo, mx0 + halfMw + DpiScale(8, dpi), btmModY, mw0 - halfMw - DpiScale(8, dpi), btmModH);
            break;
        }
        case 7: { // Charm & Gacha Box Probability Studio - Proportional simulator heights
            const int leftPanelW = contentW * 62 / 100;
            const int rightPanelW = contentW - leftPanelW - DpiScale(10, dpi);
            const int leftX = contentX;
            const int rightX = contentX + leftPanelW + DpiScale(10, dpi);

            // Left Section Top: Header & Selector Card
            const int hdrH7 = DpiScale(58, dpi);
            place(m_grpCharmHeader, leftX, contentY, leftPanelW, hdrH7);

            const int hx7 = leftX + DpiScale(12, dpi);
            const int hy7 = contentY + DpiScale(22, dpi);
            const int selLblW = DpiScale(130, dpi);
            place(m_lblCharmSelector, hx7, hy7 + DpiScale(3, dpi), selLblW, rowH);
            const int syncBtnW = DpiScale(120, dpi);
            const int refBtnW = DpiScale(64, dpi);
            const int expBtnW = DpiScale(86, dpi);
            const int cmbW = leftPanelW - DpiScale(24, dpi) - selLblW - syncBtnW - refBtnW - expBtnW - ctlGap * 4;
            int curHx7 = hx7 + selLblW + ctlGap;
            place(m_cmbCharmSelector, curHx7, hy7, cmbW, dropH); curHx7 += cmbW + ctlGap;
            place(m_btnCharmSyncItem, curHx7, hy7, syncBtnW, btnH); curHx7 += syncBtnW + ctlGap;
            place(m_btnCharmRefresh, curHx7, hy7, refBtnW, btnH); curHx7 += refBtnW + ctlGap;
            place(m_btnCharmExportCsv, curHx7, hy7, expBtnW, btnH);

            // Left Section Bottom: Rewards List
            const int tblY = contentY + hdrH7 + ctlGap;
            const int tblH = contentH - hdrH7 - ctlGap;
            place(m_grpCharmTable, leftX, tblY, leftPanelW, tblH);
            place(m_lstCharmRewards, leftX + DpiScale(10, dpi), tblY + DpiScale(22, dpi),
                  leftPanelW - DpiScale(20, dpi), tblH - DpiScale(30, dpi));

            // Right Section: Analytics, Simulation, Rebalancing
            const int statCardH = std::clamp(contentH * 24 / 100, DpiScale(105, dpi), DpiScale(135, dpi));
            const int balCardH = std::clamp(contentH * 26 / 100, DpiScale(110, dpi), DpiScale(140, dpi));
            const int simCardH = contentH - statCardH - balCardH - ctlGap * 2;

            // 1. Drop Analytics Card
            place(m_grpCharmStats, rightX, contentY, rightPanelW, statCardH);
            place(m_lblCharmStatSummary, rightX + DpiScale(12, dpi), contentY + DpiScale(20, dpi),
                  rightPanelW - DpiScale(24, dpi), DpiScale(36, dpi));
            place(m_lblCharmStatOdds, rightX + DpiScale(12, dpi), contentY + DpiScale(58, dpi),
                  rightPanelW - DpiScale(24, dpi), statCardH - DpiScale(62, dpi));

            // 2. Monte Carlo Simulator Card
            const int simY = contentY + statCardH + ctlGap;
            place(m_grpCharmSim, rightX, simY, rightPanelW, simCardH);
            const int sx7 = rightX + DpiScale(12, dpi);
            const int sw7 = rightPanelW - DpiScale(24, dpi);
            int sy7 = simY + DpiScale(22, dpi);

            place(m_lblSimTitle, sx7, sy7 + DpiScale(3, dpi), DpiScale(84, dpi), rowH);
            const int pullBtnW = (sw7 - DpiScale(84, dpi) - ctlGap * 6) / 6;
            int px7 = sx7 + DpiScale(88, dpi);
            place(m_btnSimPull1, px7, sy7, pullBtnW, btnH); px7 += pullBtnW + ctlGap;
            place(m_btnSimPull10, px7, sy7, pullBtnW, btnH); px7 += pullBtnW + ctlGap;
            place(m_btnSimPull100, px7, sy7, pullBtnW, btnH); px7 += pullBtnW + ctlGap;
            place(m_btnSimPull1000, px7, sy7, pullBtnW, btnH); px7 += pullBtnW + ctlGap;
            place(m_btnSimJackpot, px7, sy7, pullBtnW + DpiScale(12, dpi), btnH); px7 += pullBtnW + DpiScale(12, dpi) + ctlGap;
            place(m_btnSimReset, px7, sy7, sw7 - (px7 - sx7), btnH);
            sy7 += btnH + DpiScale(6, dpi);

            place(m_edtSimLog, sx7, sy7, sw7, std::max(40, simCardH - (sy7 - simY) - DpiScale(8, dpi)));

            // 3. Drop Weight Tuning Card
            const int balY = simY + simCardH + ctlGap;
            place(m_grpCharmBalance, rightX, balY, rightPanelW, balCardH);
            const int bx7 = rightX + DpiScale(12, dpi);
            const int bw7 = rightPanelW - DpiScale(24, dpi);
            int by7 = balY + DpiScale(22, dpi);

            place(m_lblCharmTargetWeight, bx7, by7 + DpiScale(3, dpi), DpiScale(130, dpi), rowH);
            const int normBtnW = DpiScale(140, dpi);
            place(m_edtCharmTargetWeight, bx7 + DpiScale(134, dpi), by7, bw7 - DpiScale(134, dpi) - normBtnW - ctlGap, rowH);
            place(m_btnCharmNormalize, bx7 + bw7 - normBtnW, by7, normBtnW, btnH);
            by7 += rowH + DpiScale(8, dpi);

            place(m_lblCharmEditWeight, bx7, by7 + DpiScale(3, dpi), DpiScale(130, dpi), rowH);
            const int applyBtnW = DpiScale(140, dpi);
            place(m_edtCharmEditWeight, bx7 + DpiScale(134, dpi), by7, bw7 - DpiScale(134, dpi) - applyBtnW - ctlGap, rowH);
            place(m_btnCharmApplyWeight, bx7 + bw7 - applyBtnW, by7, applyBtnW, btnH);
            break;
        }
    }

    ::EndDeferWindowPos(hdwp);

    // Keep active groupboxes behind sibling controls so they never occlude child widgets
    std::vector<CWnd*> activeGrps = { &m_grpBrowser, &m_grpSummary, &m_grpBottomActions };
    switch (m_activeTab) {
        case 0: activeGrps.push_back(&m_grpBasicProps); activeGrps.push_back(&m_grpJobs); break;
        case 1: activeGrps.push_back(&m_grpNameMid); activeGrps.push_back(&m_grpColorTags); activeGrps.push_back(&m_grpDescMid); break;
        case 2: activeGrps.push_back(&m_grpArchetypes); activeGrps.push_back(&m_grpStateRows); break;
        case 3: activeGrps.push_back(&m_grpViewport); activeGrps.push_back(&m_grpVisualCloner); break;
        case 4: activeGrps.push_back(&m_grpRawInspector); activeGrps.push_back(&m_grpRawEdit); break;
        case 5: activeGrps.push_back(&m_grpTierGen); activeGrps.push_back(&m_grpSeriesGen); activeGrps.push_back(&m_grpBatchEdit); break;
        case 6: activeGrps.push_back(&m_grpTools); activeGrps.push_back(&m_grpJsonPacks); activeGrps.push_back(&m_grpModDrafts); break;
        case 7: activeGrps.push_back(&m_grpCharmHeader); activeGrps.push_back(&m_grpCharmTable); activeGrps.push_back(&m_grpCharmStats);
                activeGrps.push_back(&m_grpCharmSim); activeGrps.push_back(&m_grpCharmBalance); break;
        default: break;
    }
    for (CWnd* grp : activeGrps) {
        if (grp && grp->GetSafeHwnd()) {
            ::SetWindowPos(grp->GetSafeHwnd(), HWND_BOTTOM, 0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOCOPYBITS);
        }
    }
}

void CMainFrame::SetWorkspaceControlsEnabled(bool enabled) {
    for (CWnd* w : { static_cast<CWnd*>(&m_edtSearch), static_cast<CWnd*>(&m_btnClearSearch),
                     static_cast<CWnd*>(&m_cmbTableFilter), static_cast<CWnd*>(&m_cmbGroupFilter),
                     static_cast<CWnd*>(&m_cmbRankFilter), static_cast<CWnd*>(&m_chkModifiedOnly),
                     static_cast<CWnd*>(&m_lstItems), static_cast<CWnd*>(&m_btnNewItem),
                     static_cast<CWnd*>(&m_btnDupItem), static_cast<CWnd*>(&m_btnDelItem),
                     static_cast<CWnd*>(&m_btnApplyItem), static_cast<CWnd*>(&m_btnRevertItem),
                     static_cast<CWnd*>(&m_btnCommitDisk), static_cast<CWnd*>(&m_btnHdrNew),
                     static_cast<CWnd*>(&m_btnHdrDup), static_cast<CWnd*>(&m_btnHdrUndo),
                     static_cast<CWnd*>(&m_btnHdrAudit), static_cast<CWnd*>(&m_btnHdrVerify),
                     static_cast<CWnd*>(&m_btnHdrCommit), static_cast<CWnd*>(&m_tabCtrl) }) {
        if (w && w->GetSafeHwnd()) w->EnableWindow(enabled ? TRUE : FALSE);
    }
}

void CMainFrame::StartAsyncWorkspaceLoad(const std::filesystem::path& resourcePath) {
    if (m_loadingWorkspace.load()) return;

    if (m_loadWorker.joinable()) {
        m_loadWorker.join();
    }

    m_loadingWorkspace.store(true);
    m_pendingPath = resourcePath;
    m_filteredItems.clear();
    m_lstItems.SetItemCountEx(0, LVSICF_NOINVALIDATEALL);
    m_lstItems.Invalidate(FALSE);
    LoadItemIntoEditor(nullptr);
    SetWorkspaceControlsEnabled(false);
    UpdateWindowCaption();

    UpdateStatusBar(std::format("Loading Dragon Nest tables from '{}' ...", resourcePath.string()));

    HWND hwnd = GetSafeHwnd();
    m_loadWorker = std::jthread([this, hwnd, resourcePath]() {
        size_t recovered = 0;
        const bool ok = m_engine.OpenWorkspace(resourcePath, &recovered, [hwnd]() {
            if (::IsWindow(hwnd)) {
                ::PostMessageW(hwnd, kMsgModelIndexReady, 0, 0);
            }
        });
        m_lastRecoveredDrafts = recovered;
        if (::IsWindow(hwnd)) {
            ::PostMessageW(hwnd, kMsgWorkspaceLoadFinished, ok ? 1 : 0, 0);
        }
    });
}

LRESULT CMainFrame::OnWorkspaceLoadFinished(WPARAM wParam, LPARAM /*lParam*/) {
    if (m_loadWorker.joinable()) {
        m_loadWorker.join();
    }
    m_loadingWorkspace.store(false);
    SetWorkspaceControlsEnabled(true);
    UpdateWindowCaption();

    if (wParam == 0) {
        UpdateStatusBar(std::format("Failed to load workspace '{}'. Verify ext/itemtable*.dnt and uistring/uistring.xml exist.",
                                    m_pendingPath.string()));
        MessageBoxW(Utf8ToWide(std::format("Could not load Dragon Nest resource workspace from:\n{}\n\n"
                                           "Make sure the folder contains 'ext/itemtable*.dnt' and 'uistring/uistring.xml'.",
                                           m_pendingPath.string())),
                    L"Workspace Load Failed", MB_ICONWARNING | MB_OK);
        return 0;
    }

    PopulateSubclassesTree();
    RefreshTableFilterCombo();
    RunItemSearch(-1);

    if (!m_filteredItems.empty()) {
        m_lstItems.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }

    std::string msg = std::format("Loaded {} items from {} itemtable*.dnt files in {:.0f} ms ({})",
                                  m_engine.Items().GetItemCount(),
                                  m_engine.Items().GetItemTableCount(),
                                  m_engine.Items().GetLastLoadMilliseconds(),
                                  m_engine.Items().GetResourcePath().string());
    if (m_lastRecoveredDrafts > 0) {
        msg += std::format("  |  Recovered {} SQLite draft(s)!", m_lastRecoveredDrafts);
    }
    UpdateStatusBar(msg);
    Invalidate(FALSE);
    return 0;
}

LRESULT CMainFrame::OnModelIndexReady(WPARAM /*wParam*/, LPARAM /*lParam*/) {
    if (m_loadingWorkspace.load() || m_selectedItemId <= 0 || m_previewWnd.HasLoadedModel()) {
        return 0;
    }
    const Data::DNItem* item = m_engine.Items().GetItem(m_selectedItemId);
    if (item && (item->rawType == 0 || item->rawType == 1)) {
        Graphics::ImageBuffer iconBuf;
        const bool hasIcon = m_engine.GetItemIcon(item->iconIndex, iconBuf);
        Engine::ModelAssetBundle modelBundle = m_engine.LoadModelForItem(*item);
        if (modelBundle.hasModel) {
            m_previewWnd.SetPreviewData(item, hasIcon ? &iconBuf : nullptr, std::move(modelBundle));
        }
    }
    return 0;
}

void CMainFrame::RefreshTableFilterCombo() {
    m_cmbTableFilter.ResetContent();
    const auto& im = m_engine.Items();
    m_cmbTableFilter.AddString(Utf8ToWide(std::format("All Item Tables ({} files, {} items)",
                                                      im.GetItemTableCount(), im.GetItemCount())));
    for (size_t i = 0; i < im.GetItemTableCount(); ++i) {
        const auto& t = im.GetItemTable(i);
        m_cmbTableFilter.AddString(Utf8ToWide(std::format("{}  ({} rows)", t.GetFileName(), t.GetRowCount())));
    }
    m_cmbTableFilter.SetCurSel(0);
}

void CMainFrame::UpdateBrowserSortHeaders() {
    const wchar_t* colNames[6] = { L"ID", L"Item Name", L"Rank", L"Lv", L"Type / Slot", L"Source DNT" };
    for (int c = 0; c < 6; ++c) {
        CString hdr = colNames[c];
        if (static_cast<uint8_t>(m_sortBy) == static_cast<uint8_t>(c)) {
            hdr += m_sortAscending ? L" ▲" : L" ▼";
        }
        LVCOLUMNW lvc{};
        lvc.mask = LVCF_TEXT;
        lvc.pszText = const_cast<wchar_t*>(hdr.GetString());
        m_lstItems.SetColumn(c, &lvc);
    }
}

void CMainFrame::RunItemSearch(int32_t preserveSelectedId) {
    if (m_loadingWorkspace.load() || !m_engine.IsWorkspaceLoaded()) {
        m_filteredItems.clear();
        m_lstItems.SetItemCountEx(0);
        m_lblMatchCount.SetWindowTextW(L"0 items");
        return;
    }

    Data::ItemSearchFilter filter;
    filter.keyword = GetEditUtf8(m_edtSearch);
    const int tblSel = m_cmbTableFilter.GetCurSel();
    filter.tableFilter = (tblSel > 0) ? (tblSel - 1) : -1;
    const int grpSel = m_cmbGroupFilter.GetCurSel();
    filter.groupFilter = (grpSel > 0) ? (grpSel - 1) : -1;
    const int rnkSel = m_cmbRankFilter.GetCurSel();
    filter.rankFilter = (rnkSel > 0) ? (rnkSel - 1) : -1;
    filter.modifiedOnly = (m_chkModifiedOnly.GetCheck() == BST_CHECKED);
    filter.sortBy = m_sortBy;
    filter.sortAscending = m_sortAscending;
    filter.limit = 0;

    auto res = m_engine.Items().Search(filter);
    m_filteredItems = std::move(res.items);

    m_lstItems.SetItemCountEx(static_cast<int>(m_filteredItems.size()), LVSICF_NOINVALIDATEALL);
    m_lstItems.Invalidate(FALSE);

    const std::string countText = std::format("Showing {} of {} total items{}",
                                              m_filteredItems.size(),
                                              m_engine.Items().GetItemCount(),
                                              filter.modifiedOnly ? " [Modified Filter]" : "");
    m_lblMatchCount.SetWindowTextW(Utf8ToWide(countText));

    const std::string batchInfo = std::format("Target: {} filtered item{} in browser",
                                              m_filteredItems.size(),
                                              m_filteredItems.size() == 1 ? "" : "s");
    m_lblBatchTargetInfo.SetWindowTextW(Utf8ToWide(batchInfo));

    if (preserveSelectedId > 0) {
        for (size_t i = 0; i < m_filteredItems.size(); ++i) {
            if (m_filteredItems[i]->id == preserveSelectedId) {
                const int row = static_cast<int>(i);
                m_lstItems.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                m_lstItems.EnsureVisible(row, FALSE);
                return;
            }
        }
    }
}

void CMainFrame::SelectAndRevealItem(int32_t itemId) {
    if (itemId <= 0) return;
    const auto* item = m_engine.Items().GetItem(itemId);
    if (!item) return;

    // Clear filters if needed so the item appears in the browser
    bool needRefilter = false;
    if (m_chkModifiedOnly.GetCheck() == BST_CHECKED && !m_engine.Items().IsItemModified(itemId)) {
        m_chkModifiedOnly.SetCheck(BST_UNCHECKED);
        needRefilter = true;
    }
    if (m_cmbTableFilter.GetCurSel() > 0 &&
        static_cast<size_t>(m_cmbTableFilter.GetCurSel() - 1) != item->tableIndex) {
        m_cmbTableFilter.SetCurSel(0);
        needRefilter = true;
    }
    if (needRefilter) {
        RunItemSearch(itemId);
    }

    for (size_t i = 0; i < m_filteredItems.size(); ++i) {
        if (m_filteredItems[i]->id == itemId) {
            const int r = static_cast<int>(i);
            m_lstItems.SetItemState(r, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            m_lstItems.EnsureVisible(r, FALSE);
            LoadItemIntoEditor(item);
            return;
        }
    }

    // Direct selection if outside filtered set
    LoadItemIntoEditor(item);
}

void CMainFrame::OnSearchTextChanged() {
    if (m_loadingWorkspace.load()) return;
    SetTimer(kSearchDebounceTimer, 80, nullptr);
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == kSearchDebounceTimer) {
        KillTimer(kSearchDebounceTimer);
        RunItemSearch(m_selectedItemId);
        return;
    }
    CFrameWnd::OnTimer(nIDEvent);
}

void CMainFrame::OnClearSearch() {
    m_edtSearch.SetWindowTextW(L"");
    KillTimer(kSearchDebounceTimer);
    RunItemSearch(m_selectedItemId);
}

void CMainFrame::OnFilterChanged() {
    if (m_loadingWorkspace.load()) return;
    RunItemSearch(m_selectedItemId);
}

void CMainFrame::OnItemListColumnClick(NMHDR* pNMHDR, LRESULT* pResult) {
    auto* nmlv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    *pResult = 0;
    if (nmlv->iSubItem >= 0 && nmlv->iSubItem < 6) {
        const auto clickedCol = static_cast<Data::ItemSortColumn>(nmlv->iSubItem);
        if (m_sortBy == clickedCol) {
            m_sortAscending = !m_sortAscending;
        } else {
            m_sortBy = clickedCol;
            m_sortAscending = true;
        }
        UpdateBrowserSortHeaders();
        RunItemSearch(m_selectedItemId);
    }
}

void CMainFrame::OnItemListGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult) {
    auto* dispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    LVITEMW& lvItem = dispInfo->item;

    if ((lvItem.mask & LVIF_TEXT) && lvItem.iItem >= 0 &&
        static_cast<size_t>(lvItem.iItem) < m_filteredItems.size()) {
        const Data::DNItem* item = m_filteredItems[static_cast<size_t>(lvItem.iItem)];
        const bool isMod = m_engine.Items().IsItemModified(item->id);
        CString text;
        switch (lvItem.iSubItem) {
            case 0:
                text = Utf8ToWide(std::format("{}{}", isMod ? "*" : "", item->id));
                break;
            case 1:
                text = Utf8ToWide(item->name);
                break;
            case 2:
                text = Utf8ToWide(Data::ItemManager::RankName(item->rank));
                break;
            case 3:
                text = Utf8ToWide(std::to_string(item->levelLimit));
                break;
            case 4: {
                const std::string_view sub = Data::ItemManager::EquipSubTypeName(*item);
                text = sub.empty() ? Utf8ToWide(Data::ItemManager::TypeName(item->rawType)) : Utf8ToWide(sub);
                break;
            }
            case 5:
                text = Utf8ToWide(m_engine.Items().GetItemTableName(item->tableIndex));
                break;
            default:
                break;
        }
        ::lstrcpynW(lvItem.pszText, text.GetString(), lvItem.cchTextMax);
    }
    *pResult = 0;
}

void CMainFrame::OnItemListCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) {
    auto* cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
    *pResult = CDRF_DODEFAULT;

    if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) {
        *pResult = CDRF_NOTIFYITEMDRAW;
        return;
    }
    if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        *pResult = CDRF_NOTIFYSUBITEMDRAW;
        return;
    }
    if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
        const int row = static_cast<int>(cd->nmcd.dwItemSpec);
        if (row >= 0 && static_cast<size_t>(row) < m_filteredItems.size()) {
            const bool isSelected = (m_lstItems.GetItemState(row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
            if (isSelected) {
                // Own the selection look: accent while focused, soft accent when the list lost focus.
                const bool focused = ::GetFocus() == m_lstItems.GetSafeHwnd();
                cd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
                cd->clrTextBk = focused ? Theme::kAccent : Theme::kAccentSoft;
                cd->clrText = focused ? RGB(255, 255, 255) : Theme::kText;
            } else {
                const Data::DNItem* item = m_filteredItems[static_cast<size_t>(row)];
                const bool isMod = m_engine.Items().IsItemModified(item->id);
                if (cd->iSubItem == 0 && isMod) {
                    cd->clrText = RGB(5, 150, 105);
                } else if (cd->iSubItem == 2 || (cd->iSubItem == 1 && item->rank >= 1)) {
                    cd->clrText = RankTextColor(item->rank, false);
                } else {
                    cd->clrText = Theme::kText;
                }
                cd->clrTextBk = isMod ? RGB(242, 252, 246) : ((row % 2 == 0) ? Theme::kSurface : RGB(245, 245, 248));
            }
        }
        *pResult = CDRF_DODEFAULT;
    }
}

void CMainFrame::OnItemListSelectionChanged(NMHDR* pNMHDR, LRESULT* pResult) {
    auto* nmlv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    *pResult = 0;
    if ((nmlv->uChanged & LVIF_STATE) && (nmlv->uNewState & LVIS_SELECTED)) {
        if (nmlv->iItem >= 0 && static_cast<size_t>(nmlv->iItem) < m_filteredItems.size()) {
            LoadItemIntoEditor(m_filteredItems[static_cast<size_t>(nmlv->iItem)]);
        }
    }
}

void CMainFrame::OnItemListRightClick(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    *pResult = 0;
    const int sel = m_lstItems.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0 || static_cast<size_t>(sel) >= m_filteredItems.size()) return;
    const Data::DNItem* item = m_filteredItems[static_cast<size_t>(sel)];
    if (!item) return;

    CPoint pt;
    ::GetCursorPos(&pt);

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenuW(MF_STRING, ID_CMD_ITEM_APPLY, L"&Apply Edits\tCtrl+Enter");
    menu.AppendMenuW(MF_STRING, ID_CMD_ITEM_DUPLICATE, L"&Duplicate Item...\tCtrl+D");
    menu.AppendMenuW(MF_STRING, ID_CTX_FORK_MIDS, L"Fork &Exclusive MIDs (Safe Rename)");
    menu.AppendMenuW(MF_SEPARATOR);
    menu.AppendMenuW(MF_STRING, ID_CTX_USE_AS_DONOR, L"Set as &Visual Donor Item");
    menu.AppendMenuW(MF_STRING, ID_CTX_GEN_TIER_SET, L"Generate Multi-&Rank Tier Set...");
    menu.AppendMenuW(MF_STRING, ID_CTX_GEN_CLASS_SERIES, L"Generate Multi-&Class Series...");
    menu.AppendMenuW(MF_SEPARATOR);
    menu.AppendMenuW(MF_STRING, ID_CTX_COPY_ID, L"Copy #&ID to Clipboard");
    menu.AppendMenuW(MF_STRING, ID_CTX_COPY_JSON, L"Copy Item &JSON to Clipboard");
    if (item) {
        menu.AppendMenuW(MF_SEPARATOR);
        if (m_engine.Items().IsDragonJewel(*item) || m_engine.Items().HasJewelSockets(*item)) {
            menu.AppendMenuW(MF_STRING, ID_CTX_OPEN_DRAGON_JEWEL, L"Open in &Dragon Jewel Studio...\tCtrl+J");
        }
        if (item->setItemId > 0) {
            menu.AppendMenuW(MF_STRING, ID_CTX_OPEN_SET_ITEM, L"Inspect &Set Item Bonuses...\tCtrl+Shift+S");
        }
        if (item->enchantId > 0) {
            menu.AppendMenuW(MF_STRING, ID_CTX_OPEN_ENCHANT, L"Inspect &Enchant Curve...\tCtrl+Shift+E");
        }
        if (m_engine.Items().HasCharm(*item)) {
            menu.AppendMenuW(MF_STRING, ID_CMD_TOOLS_CHARM_CALC, L"Open in &Charm / Gacha Calculator...\tCtrl+G");
        }
    }
    menu.AppendMenuW(MF_SEPARATOR);
    menu.AppendMenuW(MF_STRING, ID_CMD_ITEM_DELETE, L"De&lete Item Row");

    const UINT cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    switch (cmd) {
        case ID_CMD_ITEM_APPLY:     OnItemApply(); break;
        case ID_CMD_ITEM_DUPLICATE: OnItemDuplicate(); break;
        case ID_CTX_FORK_MIDS:      OnForkExclusiveMids(); break;
        case ID_CTX_USE_AS_DONOR:   OnUseSelectedAsDonor(); break;
        case ID_CTX_GEN_TIER_SET:
            ShowActiveEditorTab(5);
            m_edtTierBaseName.SetWindowTextW(Utf8ToWide(item->name));
            break;
        case ID_CTX_GEN_CLASS_SERIES:
            ShowActiveEditorTab(5);
            m_edtSeriesTmpl.SetWindowTextW(Utf8ToWide(std::format("{{CLASS}} {}", item->name)));
            break;
        case ID_CTX_COPY_ID:
            CopyUtf8ToClipboard(GetSafeHwnd(), std::to_string(item->id));
            UpdateStatusBar(std::format("Copied #ID {} to clipboard.", item->id));
            break;
        case ID_CTX_COPY_JSON: {
            const std::string json = m_engine.Items().ExportItemsToJson({ item->id });
            CopyUtf8ToClipboard(GetSafeHwnd(), json);
            UpdateStatusBar(std::format("Copied item #{} JSON to clipboard.", item->id));
            break;
        }
        case ID_CMD_ITEM_DELETE: OnItemDelete(); break;
        default: break;
    }
}

namespace {

bool PickSaveFileDialog(HWND owner, const std::string& defaultFileName, const std::string& defaultExt, std::filesystem::path& outPath) {
    IFileSaveDialog* dlg = nullptr;
    HRESULT hr = ::CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return false;

    COMDLG_FILTERSPEC filter[] = {
        { L"JSON Item Pack (*.json)", L"*.json" },
        { L"All Files (*.*)", L"*.*" }
    };
    dlg->SetFileTypes(ARRAYSIZE(filter), filter);
    dlg->SetDefaultExtension(Utf8ToWide(defaultExt));
    dlg->SetFileName(Utf8ToWide(defaultFileName));

    bool picked = false;
    if (SUCCEEDED(dlg->Show(owner))) {
        IShellItem* result = nullptr;
        if (SUCCEEDED(dlg->GetResult(&result)) && result) {
            PWSTR pszPath = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath) {
                outPath = std::filesystem::path(pszPath);
                picked = true;
                ::CoTaskMemFree(pszPath);
            }
            result->Release();
        }
    }
    dlg->Release();
    return picked;
}

bool PickOpenFileDialog(HWND owner, const std::string& defaultExt, std::filesystem::path& outPath) {
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = ::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return false;

    COMDLG_FILTERSPEC filter[] = {
        { L"JSON Item Pack (*.json)", L"*.json" },
        { L"All Files (*.*)", L"*.*" }
    };
    dlg->SetFileTypes(ARRAYSIZE(filter), filter);
    dlg->SetDefaultExtension(Utf8ToWide(defaultExt));

    bool picked = false;
    if (SUCCEEDED(dlg->Show(owner))) {
        IShellItem* result = nullptr;
        if (SUCCEEDED(dlg->GetResult(&result)) && result) {
            PWSTR pszPath = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath) {
                outPath = std::filesystem::path(pszPath);
                picked = true;
                ::CoTaskMemFree(pszPath);
            }
            result->Release();
        }
    }
    dlg->Release();
    return picked;
}

} // namespace

void CMainFrame::UpdateItemSummary(const Data::DNItem* item,
                                   const std::string& sourceTableName,
                                   const std::string& detailTableName,
                                   const Graphics::ImageBuffer* iconImg,
                                   bool isModified,
                                   size_t sharedNameMidCount) {
    m_bannerIconWnd.SetIcon(iconImg);
    if (!item) {
        ClearItemSummary();
        return;
    }
    m_lblBannerName.SetWindowTextW(Utf8ToWide(item->name.empty() ? "(Unnamed Item)" : item->name));
    const std::string rankName = Data::ItemManager::RankName(item->rank);
    const std::string typeName = Data::ItemManager::TypeName(item->rawType);
    const std::string meta = std::format("Row ID: {}  |  Level: {}  |  Rank: {} ({})  |  Table: {}  |  Type: {}",
                                        item->id, item->levelLimit, item->rank, rankName, sourceTableName, typeName);
    m_lblBannerMeta.SetWindowTextW(Utf8ToWide(meta));

    std::string midInfo;
    if (isModified) midInfo += "[MODIFIED]  ";
    if (item->nameId > 0) {
        midInfo += std::format("Name MID #{}: {}", item->nameId,
                               sharedNameMidCount > 0 ? std::format("shared with {} other items", sharedNameMidCount) : "exclusive");
    }
    if (!detailTableName.empty()) {
        midInfo += std::format("  |  Detail: {}", detailTableName);
    }
    m_lblBannerMid.SetWindowTextW(Utf8ToWide(midInfo));
}

void CMainFrame::UpdateLiveSummaryPreview(const std::string& liveName,
                                         int32_t liveRank,
                                         int32_t liveIconIdx,
                                         const Graphics::ImageBuffer* iconImg,
                                         size_t sharedNameMidCount) {
    m_bannerIconWnd.SetIcon(iconImg);
    m_lblBannerName.SetWindowTextW(Utf8ToWide(liveName.empty() ? "(Unnamed Item)" : liveName));
    const std::string rankName = Data::ItemManager::RankName(liveRank);
    const std::string meta = std::format("ID: #{}  |  Live Rank: {} ({})  |  Icon Index: #{}",
                                        m_selectedItemId, liveRank, rankName, liveIconIdx);
    m_lblBannerMeta.SetWindowTextW(Utf8ToWide(meta));
}

void CMainFrame::ClearItemSummary() {
    m_bannerIconWnd.SetIcon(nullptr);
    m_lblBannerName.SetWindowTextW(L"No item selected");
    m_lblBannerMeta.SetWindowTextW(L"Select an item from the browser or click '+ New Item'.");
    m_lblBannerMid.SetWindowTextW(L"");
}

void CMainFrame::LoadItemIntoEditor(const Data::DNItem* item) {
    m_selectedItemId = item ? item->id : 0;
    m_populatingEditor = true;

    if (!item) {
        ClearItemSummary();
        m_previewWnd.ClearPreview();

        m_cmbEditRank.SetCurSel(0);
        m_edtIconIndex.SetWindowTextW(L"");
        m_edtLevel.SetWindowTextW(L"");
        m_edtHeroLevel.SetWindowTextW(L"");
        m_edtOverlap.SetWindowTextW(L"");
        m_edtSell.SetWindowTextW(L"");
        m_edtGearScore.SetWindowTextW(L"");
        m_edtDurability.SetWindowTextW(L"");
        m_chkCash.SetCheck(BST_UNCHECKED);
        m_chkStorage.SetCheck(BST_UNCHECKED);
        m_chkWStorage.SetCheck(BST_UNCHECKED);
        m_edtJobs.SetWindowTextW(L"");
        SyncClassCheckboxesFromJobString("");
        m_lblJobsResolved.SetWindowTextW(L"");

        m_edtNameMid.SetWindowTextW(L"");
        m_edtNameParam.SetWindowTextW(L"");
        m_edtNameTmpl.SetWindowTextW(L"");
        m_edtNameResolved.SetWindowTextW(L"");
        m_edtDescMid.SetWindowTextW(L"");
        m_edtDescParam.SetWindowTextW(L"");
        m_edtDescTmpl.SetWindowTextW(L"");
        m_edtDescResolved.SetWindowTextW(L"");
        m_lblMidCollisionInfo.SetWindowTextW(L"No item selected.");
        m_lblPropJewelInfo.SetWindowTextW(L"Dragon Jewel Sockets: None");
        m_btnPropJewelStudio.EnableWindow(FALSE);
        m_lblPropSetItemInfo.SetWindowTextW(L"Set Item: None");
        m_btnPropSetItemStudio.EnableWindow(FALSE);
        m_lblPropEnchantInfo.SetWindowTextW(L"Enchant Curve: None");
        m_btnPropEnchantStudio.EnableWindow(FALSE);
        UpdateVisibleTabsForItem(nullptr);
        m_btnForkMids.EnableWindow(FALSE);

        for (size_t i = 0; i < Data::kStateSlotCount; ++i) {
            m_statWidgets[i].cmbType.SetCurSel(0);
            m_statWidgets[i].preservedUnknownType = -1;
            m_statWidgets[i].edtProb.SetWindowTextW(L"0.0000");
            m_statWidgets[i].edtMin.SetWindowTextW(L"0");
            m_statWidgets[i].edtMax.SetWindowTextW(L"0");
        }

        m_rawEntries.clear();
        m_lstRawCols.DeleteAllItems();
        m_selectedRawColIdx = -1;
        m_lblRawSelectedCol.SetWindowTextW(L"Select a column below to edit");
        m_edtRawCellValue.SetWindowTextW(L"");

        m_populatingEditor = false;
        UpdateWindowCaption();
        return;
    }

    // Populate banner and 3D preview
    Graphics::ImageBuffer iconTile;
    const bool hasIcon = m_engine.GetItemIcon(item->iconIndex, iconTile);
    const size_t nameCollisions = (item->nameId > 0 && m_engine.Items().CountItemsUsingMid(item->nameId) > 1)
                                      ? (m_engine.Items().CountItemsUsingMid(item->nameId) - 1)
                                      : 0;
    const bool isMod = m_engine.Items().IsItemModified(item->id);

    const std::string sourceTbl = m_engine.Items().GetItemTableName(item->tableIndex);
    std::string detailTbl;
    if (const auto* dt = m_engine.Items().GetDetailTableForItem(*item)) {
        detailTbl = dt->GetFileName();
    }

    UpdateItemSummary(
        item,
        sourceTbl,
        detailTbl,
        hasIcon ? &iconTile : nullptr,
        isMod,
        nameCollisions
    );

    if (m_activeTab == 3) {
        auto bundle = m_engine.LoadModelForItem(*item);
        m_previewWnd.SetPreviewData(item, hasIcon ? &iconTile : nullptr, std::move(bundle));
    } else {
        m_previewWnd.UpdateIconOnly(item->iconIndex, item->name, item->rank, hasIcon ? &iconTile : nullptr);
    }
    m_cmbEditRank.SetCurSel(std::clamp<int>(item->rank, 0, 6));
    m_edtIconIndex.SetWindowTextW(Utf8ToWide(std::to_string(item->iconIndex)));
    m_edtLevel.SetWindowTextW(Utf8ToWide(std::to_string(item->levelLimit)));
    m_edtHeroLevel.SetWindowTextW(Utf8ToWide(std::to_string(item->heroLevelLimit)));
    m_edtOverlap.SetWindowTextW(Utf8ToWide(std::to_string(item->overlapCount)));
    m_edtSell.SetWindowTextW(Utf8ToWide(std::to_string(item->sellAmount)));
    m_edtGearScore.SetWindowTextW(Utf8ToWide(std::to_string(item->gearScore)));
    m_edtDurability.SetWindowTextW(Utf8ToWide(std::to_string(item->durability)));
    m_chkCash.SetCheck(item->isCash ? BST_CHECKED : BST_UNCHECKED);
    m_chkStorage.SetCheck(item->ableStorage ? BST_CHECKED : BST_UNCHECKED);
    m_chkWStorage.SetCheck(item->ableWStorage ? BST_CHECKED : BST_UNCHECKED);
    m_edtJobs.SetWindowTextW(Utf8ToWide(item->jobClass));
    SyncClassCheckboxesFromJobString(item->jobClass);
    m_lblJobsResolved.SetWindowTextW(Utf8ToWide(m_engine.Items().DescribeJobs(item->jobClass)));
    m_edtNameMid.SetWindowTextW(Utf8ToWide(std::to_string(item->nameId)));
    m_edtNameParam.SetWindowTextW(Utf8ToWide(item->nameParam));
    m_edtNameTmpl.SetWindowTextW(Utf8ToWide(item->nameTemplate));
    m_edtDescMid.SetWindowTextW(Utf8ToWide(std::to_string(item->descId)));
    m_edtDescParam.SetWindowTextW(Utf8ToWide(item->descParam));
    m_edtDescTmpl.SetWindowTextW(Utf8ToWide(item->descTemplate));
    UpdateLiveResolvedPreviews();
    UpdateMidCollisionBanner();
    UpdateVisibleTabsForItem(item);

    // Linked DNT tables: Dragon Jewels & Sockets
    if (m_engine.Items().IsDragonJewel(*item)) {
        const auto* jInfo = m_engine.Items().GetDragonJewelInfo(item->id);
        std::string jSummary = jInfo ? std::format("Dragon Jewel: {} (Type {})", jInfo->GetTypeName(), jInfo->type)
                                     : "Dragon Jewel Item";
        m_lblPropJewelInfo.SetWindowTextW(Utf8ToWide(jSummary));
        m_btnPropJewelStudio.EnableWindow(TRUE);
    } else if (m_engine.Items().HasJewelSockets(*item)) {
        const auto* sCfg = m_engine.Items().GetItemJewelSlots(item->dragonJewelType);
        std::string sSummary = sCfg ? std::format("Sockets: {} [{}]", sCfg->GetActiveSlotCount(), sCfg->GetSummary())
                                    : "Dragon Jewel Sockets";
        m_lblPropJewelInfo.SetWindowTextW(Utf8ToWide(sSummary));
        m_btnPropJewelStudio.EnableWindow(TRUE);
    } else {
        m_lblPropJewelInfo.SetWindowTextW(L"Dragon Jewels: None");
        m_btnPropJewelStudio.EnableWindow(FALSE);
    }

    // Linked DNT tables: Set Items
    if (item->setItemId > 0) {
        const auto* sGrp = m_engine.Items().GetSetItemGroup(item->setItemId);
        std::string setSummary = sGrp ? std::format("Set #{}: {} ({} pcs)", item->setItemId, sGrp->setName, sGrp->memberItemIds.size())
                                      : std::format("Set #{}", item->setItemId);
        m_lblPropSetItemInfo.SetWindowTextW(Utf8ToWide(setSummary));
        m_btnPropSetItemStudio.EnableWindow(TRUE);
    } else {
        m_lblPropSetItemInfo.SetWindowTextW(L"Set Item: None");
        m_btnPropSetItemStudio.EnableWindow(FALSE);
    }

    // Linked DNT tables: Enchantment Curves
    if (item->enchantId > 0) {
        const auto encCurve = m_engine.Items().GetEnchantCurve(item->enchantId);
        std::string encSummary = encCurve.steps.empty() ? std::format("Enchant Curve #{} (No steps)", item->enchantId)
                                                        : std::format("Enchant Curve #{}: Max +{}", item->enchantId, encCurve.steps.back().level);
        m_lblPropEnchantInfo.SetWindowTextW(Utf8ToWide(encSummary));
        m_btnPropEnchantStudio.EnableWindow(TRUE);
    } else {
        m_lblPropEnchantInfo.SetWindowTextW(L"Enchant: Not enhanceable");
        m_btnPropEnchantStudio.EnableWindow(FALSE);
    };
    PopulateStatWidgetsFromItem(*item);
    if (m_activeTab == 4) {
        PopulateRawInspectorList();
    } else if (m_activeTab == 7) {
        PopulateCharmTabForCurrentItem();
    }
    CString curTierName;
    m_edtTierBaseName.GetWindowTextW(curTierName);
    if (curTierName.IsEmpty()) {
        m_edtTierBaseName.SetWindowTextW(Utf8ToWide(item->name));
    }
    CString curSeriesTmpl;
    m_edtSeriesTmpl.GetWindowTextW(curSeriesTmpl);
    if (curSeriesTmpl.IsEmpty()) {
        m_edtSeriesTmpl.SetWindowTextW(Utf8ToWide(std::format("{{CLASS}} {}", item->name)));
    }

    m_populatingEditor = false;
    UpdateWindowCaption();
    UpdateStatusBar(std::format("Loaded item #{} - {}", item->id, item->name));
}

bool CMainFrame::CollectItemFromEditor(Data::DNItem& outItem, std::string& outError) const {
    const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
    if (!cur) {
        outError = "No item is currently loaded in the editor.";
        return false;
    }

    outItem = *cur;
    outItem.rank = static_cast<int32_t>(std::clamp(m_cmbEditRank.GetCurSel(), 0, 6));
    outItem.iconIndex = ParseIntOr(m_edtIconIndex, cur->iconIndex);
    outItem.levelLimit = ParseIntOr(m_edtLevel, cur->levelLimit);
    outItem.heroLevelLimit = ParseIntOr(m_edtHeroLevel, cur->heroLevelLimit);
    outItem.overlapCount = ParseIntOr(m_edtOverlap, cur->overlapCount);
    outItem.sellAmount = ParseIntOr(m_edtSell, cur->sellAmount);
    outItem.gearScore = ParseIntOr(m_edtGearScore, cur->gearScore);
    outItem.durability = ParseIntOr(m_edtDurability, cur->durability);
    outItem.isCash = (m_chkCash.GetCheck() == BST_CHECKED);
    outItem.ableStorage = (m_chkStorage.GetCheck() == BST_CHECKED);
    outItem.ableWStorage = (m_chkWStorage.GetCheck() == BST_CHECKED);
    outItem.jobClass = GetEditUtf8(m_edtJobs);
    outItem.nameId = ParseUIntOr(m_edtNameMid, cur->nameId);
    outItem.nameParam = GetEditUtf8(m_edtNameParam);
    outItem.nameTemplate = GetEditUtf8(m_edtNameTmpl);
    outItem.descId = ParseUIntOr(m_edtDescMid, cur->descId);
    outItem.descParam = GetEditUtf8(m_edtDescParam);
    outItem.descTemplate = GetEditUtf8(m_edtDescTmpl);
    outItem.name = m_engine.Items().ResolveTemplate(outItem.nameTemplate, outItem.nameParam);
    outItem.description = m_engine.Items().ResolveTemplate(outItem.descTemplate, outItem.descParam);
    for (size_t i = 0; i < Data::kStateSlotCount; ++i) {
        const auto& row = m_statWidgets[i];
        auto& s = outItem.states[i];
        const int sel = row.cmbType.GetCurSel();
        if (sel <= 0) {
            s.type = (row.preservedUnknownType > 0 && sel == 0) ? row.preservedUnknownType : -1;
        } else {
            s.type = Data::ItemManager::StateTypeFromIndex(sel);
        }
        s.genProb = ParseFloatOr(row.edtProb, 0.0f);
        s.minValue = GetEditUtf8(row.edtMin);
        if (s.minValue.empty()) s.minValue = "0";
        s.maxValue = GetEditUtf8(row.edtMax);
        if (s.maxValue.empty()) s.maxValue = "0";
    }

    return true;
}

void CMainFrame::PopulateStatWidgetsFromItem(const Data::DNItem& item) {
    for (size_t i = 0; i < Data::kStateSlotCount; ++i) {
        auto& row = m_statWidgets[i];
        const auto& s = item.states[i];
        const int idx = Data::ItemManager::StateIndexFromType(s.type);
        if (idx >= 0) {
            row.cmbType.SetCurSel(idx);
            row.preservedUnknownType = -1;
        } else {
            row.cmbType.SetCurSel(0);
            row.preservedUnknownType = s.type;
        }
        row.edtProb.SetWindowTextW(Utf8ToWide(std::format("{:.4f}", s.genProb)));
        row.edtMin.SetWindowTextW(Utf8ToWide(s.minValue));
        row.edtMax.SetWindowTextW(Utf8ToWide(s.maxValue));
    }
}

void CMainFrame::SyncClassCheckboxesFromJobString(std::string_view jobStr) {
    m_syncingClasses = true;

    std::unordered_set<int32_t> jobSet;
    size_t start = 0;
    while (start < jobStr.size()) {
        const size_t end = jobStr.find(';', start);
        const std::string_view token = jobStr.substr(start, (end == std::string_view::npos) ? jobStr.size() - start : end - start);
        start = (end == std::string_view::npos) ? jobStr.size() : end + 1;

        int32_t jid = 0;
        const auto [p, ec] = std::from_chars(token.data(), token.data() + token.size(), jid);
        if (ec == std::errc{} && jid > 0) {
            jobSet.insert(jid);
        }
    }

    bool all10 = true;
    for (size_t i = 0; i < 10; ++i) {
        const bool checked = jobSet.contains(kBaseClassIds[i]);
        m_chkBaseClasses[i].SetCheck(checked ? BST_CHECKED : BST_UNCHECKED);
        if (!checked) all10 = false;
    }
    m_chkClassAll.SetCheck((all10 || jobStr.empty() || jobStr == "0") ? BST_CHECKED : BST_UNCHECKED);

    if (m_treeSubclasses.GetSafeHwnd()) {
        auto syncTree = [&](auto& self, HTREEITEM hItem) -> void {
            while (hItem) {
                const auto jid = static_cast<int32_t>(m_treeSubclasses.GetItemData(hItem));
                m_treeSubclasses.SetCheck(hItem, (jid > 0 && jobSet.contains(jid)) ? TRUE : FALSE);

                HTREEITEM hChild = m_treeSubclasses.GetChildItem(hItem);
                if (hChild) {
                    self(self, hChild);
                }
                hItem = m_treeSubclasses.GetNextSiblingItem(hItem);
            }
        };
        HTREEITEM hRoot = m_treeSubclasses.GetRootItem();
        syncTree(syncTree, hRoot);
    }

    m_syncingClasses = false;
}

void CMainFrame::PopulateSubclassesTree() {
    if (!m_treeSubclasses.GetSafeHwnd()) return;

    std::unordered_set<int32_t> checkedBefore;
    if (m_treeSubclasses.GetCount() > 0) {
        std::vector<int32_t> ids;
        CollectCheckedJobIds(ids);
        for (int32_t id : ids) checkedBefore.insert(id);
    }

    m_treeSubclasses.SetRedraw(FALSE);
    m_treeSubclasses.DeleteAllItems();

    std::unordered_map<int32_t, HTREEITEM> itemMap;

    for (const auto& entry : kDNClassHierarchy) {
        std::wstring displayName = entry.name;
        if (m_engine.IsWorkspaceLoaded()) {
            std::string dynName = m_engine.Items().DescribeJobs(std::to_string(entry.jobId));
            if (!dynName.empty() && dynName != "All Classes" && dynName.front() != '#') {
                displayName = Utf8ToWide(dynName);
            }
        }
        const std::wstring itemText = std::format(L"{} (#{})", displayName, entry.jobId);

        HTREEITEM hParent = TVI_ROOT;
        if (entry.tier > 0) {
            auto it = itemMap.find(entry.parentId);
            if (it != itemMap.end()) {
                hParent = it->second;
            }
        }

        HTREEITEM hItem = m_treeSubclasses.InsertItem(itemText.c_str(), hParent, TVI_LAST);
        m_treeSubclasses.SetItemData(hItem, static_cast<DWORD_PTR>(entry.jobId));
        itemMap[entry.jobId] = hItem;

        if (checkedBefore.contains(entry.jobId)) {
            m_treeSubclasses.SetCheck(hItem, TRUE);
        }
    }

    for (HTREEITEM h = m_treeSubclasses.GetRootItem(); h != nullptr; h = m_treeSubclasses.GetNextSiblingItem(h)) {
        m_treeSubclasses.Expand(h, TVE_EXPAND);
    }
    m_subclassesExpanded = true;
    m_btnSubclassExpand.SetWindowTextW(L"Collapse");

    m_treeSubclasses.SetRedraw(TRUE);
    m_treeSubclasses.Invalidate();
}

void CMainFrame::CollectCheckedJobIds(std::vector<int32_t>& outIds) const {
    outIds.clear();
    if (!m_treeSubclasses.GetSafeHwnd()) return;

    auto checkItemAndChildren = [&](auto& self, HTREEITEM hItem) -> void {
        while (hItem) {
            if (m_treeSubclasses.GetCheck(hItem)) {
                const auto jid = static_cast<int32_t>(m_treeSubclasses.GetItemData(hItem));
                if (jid > 0) {
                    outIds.push_back(jid);
                }
            }
            HTREEITEM hChild = m_treeSubclasses.GetChildItem(hItem);
            if (hChild) {
                self(self, hChild);
            }
            hItem = m_treeSubclasses.GetNextSiblingItem(hItem);
        }
    };
    HTREEITEM hRoot = m_treeSubclasses.GetRootItem();
    checkItemAndChildren(checkItemAndChildren, hRoot);

    for (size_t i = 0; i < 10; ++i) {
        if (m_chkBaseClasses[i].GetCheck() == BST_CHECKED) {
            const int32_t bId = kBaseClassIds[i];
            if (std::find(outIds.begin(), outIds.end(), bId) == outIds.end()) {
                outIds.push_back(bId);
            }
        }
    }
    std::sort(outIds.begin(), outIds.end());
    outIds.erase(std::unique(outIds.begin(), outIds.end()), outIds.end());
}

void CMainFrame::SetBranchChecked(HTREEITEM hItem, BOOL bCheck) {
    if (!hItem) return;
    m_treeSubclasses.SetCheck(hItem, bCheck);
    for (HTREEITEM hChild = m_treeSubclasses.GetChildItem(hItem); hChild != nullptr; hChild = m_treeSubclasses.GetNextSiblingItem(hChild)) {
        SetBranchChecked(hChild, bCheck);
    }
}

void CMainFrame::UpdateMidCollisionBanner() {
    const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
    if (!cur) {
        m_lblMidCollisionInfo.SetWindowTextW(L"No item selected.");
        m_btnForkMids.EnableWindow(FALSE);
        return;
    }

    const size_t nameCollisions = (cur->nameId > 0 && m_engine.Items().CountItemsUsingMid(cur->nameId) > 1)
                                      ? (m_engine.Items().CountItemsUsingMid(cur->nameId) - 1)
                                      : 0;
    const size_t descCollisions = (cur->descId > 0 && m_engine.Items().CountItemsUsingMid(cur->descId) > 1)
                                      ? (m_engine.Items().CountItemsUsingMid(cur->descId) - 1)
                                      : 0;

    if (nameCollisions == 0 && descCollisions == 0) {
        m_lblMidCollisionInfo.SetWindowTextW(L"[OK] Exclusive MIDs: Editing text will only affect this item.");
        m_btnForkMids.EnableWindow(FALSE);
    } else {
        const CString warn = Utf8ToWide(std::format(
            "[WARN] Shared MIDs: Name MID {} shared with {} items | Desc MID {} shared with {} items. Use 'Fork Exclusive MIDs' to rename safely.",
            cur->nameId, nameCollisions, cur->descId, descCollisions));
        m_lblMidCollisionInfo.SetWindowTextW(warn);
        m_btnForkMids.EnableWindow(TRUE);
    }
}

void CMainFrame::UpdateLiveResolvedPreviews() {
    const std::string nameTmpl = GetEditUtf8(m_edtNameTmpl);
    const std::string nameParam = GetEditUtf8(m_edtNameParam);
    const std::string resolvedName = m_engine.Items().ResolveTemplate(nameTmpl, nameParam);
    m_edtNameResolved.SetWindowTextW(Utf8ToWide(resolvedName));

    const std::string descTmpl = GetEditUtf8(m_edtDescTmpl);
    const std::string descParam = GetEditUtf8(m_edtDescParam);
    const std::string resolvedDesc = m_engine.Items().ResolveTemplate(descTmpl, descParam);
    m_edtDescResolved.SetWindowTextW(Utf8ToWide(resolvedDesc));
}

void CMainFrame::PopulateRawInspectorList() {
    m_rawEntries.clear();
    m_lstRawCols.DeleteAllItems();
    m_selectedRawColIdx = -1;
    m_lblRawSelectedCol.SetWindowTextW(L"Select a column below to inspect or edit");
    m_edtRawCellValue.SetWindowTextW(L"");

    const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
    if (!cur) return;

    const bool detailTable = (m_cmbRawSource.GetCurSel() == 1);
    const Data::DNTTable* tbl = nullptr;
    if (!detailTable) {
        if (cur->tableIndex < m_engine.Items().GetItemTableCount()) {
            tbl = &m_engine.Items().GetItemTable(cur->tableIndex);
        }
    } else {
        tbl = m_engine.Items().GetDetailTableForItem(*cur);
    }
    if (!tbl) return;
    const auto* row = tbl->GetRow(cur->id);
    if (!row) return;

    const std::string filter = GetEditUtf8(m_edtRawFilter);
    std::string lowerFilter;
    lowerFilter.reserve(filter.size());
    for (char c : filter) lowerFilter.push_back(AsciiLower(c));

    const auto& cols = tbl->GetColumns();
    for (size_t c = 0; c < cols.size(); ++c) {
        if (!lowerFilter.empty() && !ContainsAsciiCaseInsensitive(cols[c].name, lowerFilter)) {
            continue;
        }
        RawColumnEntry entry;
        entry.colIdx = c;
        entry.name = cols[c].name;
        entry.typeName = Data::DNTTable::ColumnTypeName(cols[c].type);
        entry.value = tbl->FormatCell(*row, c);
        m_rawEntries.push_back(std::move(entry));
    }

    for (int i = 0; i < static_cast<int>(m_rawEntries.size()); ++i) {
        const auto& e = m_rawEntries[static_cast<size_t>(i)];
        const int rowIdx = m_lstRawCols.InsertItem(i, Utf8ToWide(std::to_string(e.colIdx)));
        m_lstRawCols.SetItemText(rowIdx, 1, Utf8ToWide(std::format("{} ({})", e.name, e.typeName)));
        m_lstRawCols.SetItemText(rowIdx, 2, Utf8ToWide(e.value));
    }
}

void CMainFrame::RefreshModifiedItemsTabList() {
    m_modifiedListIds = m_engine.Items().GetModifiedItemIds();
    std::sort(m_modifiedListIds.begin(), m_modifiedListIds.end());
    m_lstModifiedItems.DeleteAllItems();

    for (int i = 0; i < static_cast<int>(m_modifiedListIds.size()); ++i) {
        const int32_t id = m_modifiedListIds[static_cast<size_t>(i)];
        const auto* it = m_engine.Items().GetItem(id);
        const int row = m_lstModifiedItems.InsertItem(i, Utf8ToWide(std::to_string(id)));
        if (it) {
            m_lstModifiedItems.SetItemText(row, 1, Utf8ToWide(Data::ItemManager::RankName(it->rank)));
            m_lstModifiedItems.SetItemText(row, 2, Utf8ToWide(it->name));
            m_lstModifiedItems.SetItemText(row, 3, Utf8ToWide(m_engine.Items().GetItemTableName(it->tableIndex)));
            m_lstModifiedItems.SetItemText(row, 4, L"Modified (In-Memory Draft)");
        } else {
            m_lstModifiedItems.SetItemText(row, 1, L"-");
            m_lstModifiedItems.SetItemText(row, 2, L"(Deleted / Unknown)");
            m_lstModifiedItems.SetItemText(row, 3, L"-");
            m_lstModifiedItems.SetItemText(row, 4, L"Deleted");
        }
    }
}

void CMainFrame::ExportItemPackInteractive(const std::vector<int32_t>& ids, const std::string& defaultFileName) {
    if (ids.empty()) return;
    std::filesystem::path savePath;
    if (!PickSaveFileDialog(GetSafeHwnd(), defaultFileName, "json", savePath)) return;

    const std::string json = m_engine.Items().ExportItemsToJson(ids);
    std::ofstream ofs(savePath, std::ios::binary);
    if (!ofs) {
        AfxMessageBox(L"Failed to open destination file for writing.", MB_ICONERROR);
        return;
    }
    ofs.write(json.data(), static_cast<std::streamsize>(json.size()));
    UpdateStatusBar(std::format("Exported {} items to {}", ids.size(), savePath.filename().string()));
}

void CMainFrame::UpdateStatusBar(const std::string& actionMsg) {
    if (!m_statusBar.GetSafeHwnd()) return;

    std::string text;
    if (!actionMsg.empty()) {
        text = actionMsg;
    } else if (m_engine.IsWorkspaceLoaded()) {
        text = std::format("Loaded {} items across {} item tables | {} modified",
                           m_engine.Items().GetItemCount(),
                           m_engine.Items().GetItemTableCount(),
                           m_engine.Items().GetModifiedItemCount());
    } else {
        text = "Ready. Please open a Dragon Nest resource directory.";
    }
    m_statusBar.SetPaneText(0, Utf8ToWide(text));
}

void CMainFrame::UpdateWindowCaption() {
    std::wstring title = L"DNItemMaker - Dragon Nest DNT Studio";
    if (m_engine.IsWorkspaceLoaded()) {
        const size_t totalItems = m_engine.Items().GetItemCount();
        const size_t modCount = m_engine.Items().GetModifiedItemCount();
        if (modCount > 0) {
            title += Utf8ToWide(std::format(" [{} items - {} MODIFIED *]", totalItems, modCount));
        } else {
            title += Utf8ToWide(std::format(" [{} items loaded]", totalItems));
        }
    }
    SetWindowTextW(title.c_str());
}

void CMainFrame::OnFileOpenWorkspace() {
    std::filesystem::path folder;
    if (PickFolderDialog(GetSafeHwnd(), m_engine.ResourcePath(), folder)) {
        StartAsyncWorkspaceLoad(folder);
    }
}

void CMainFrame::OnFileReloadWorkspace() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    if (m_engine.Items().GetModifiedItemCount() > 0) {
        if (AfxMessageBox(L"Reloading workspace will discard unsaved in-memory edits.\r\nContinue?",
                          MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return;
        }
    }
    StartAsyncWorkspaceLoad(m_engine.ResourcePath());
}

void CMainFrame::OnFileCommitToDisk() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    const size_t modCount = m_engine.Items().GetModifiedItemCount();
    if (modCount == 0) {
        AfxMessageBox(L"No modified tables or items to commit.", MB_ICONINFORMATION);
        return;
    }

    const CString prompt = Utf8ToWide(std::format(
        "Commit {} modified items across DNT tables and strings to disk?\r\n"
        "Original files will be safely backed up before writing.", modCount));
    if (AfxMessageBox(prompt, MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    UpdateStatusBar("Committing modified tables and strings to disk...");
    std::string report;
    const bool ok = m_engine.CommitAllToDisk(report);
    UpdateWindowCaption();
    UpdateStatusBar(ok ? "All changes committed to disk successfully." : "Commit encountered errors.");
    CReportDlg dlg(std::string(ok ? "Commit to Disk Successful [OK]" : "Commit to Disk Warning [CHECK]"),
                   std::string(ok ? "All modified DNT tables and UIStrings were written safely to disk."
                                  : "Some tables could not be written to disk. Check details below."),
                   report, this);
    dlg.DoModal();
}

void CMainFrame::OnFileDiscardDrafts() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    if (AfxMessageBox(L"Discard all session recovery drafts and reload workspace from disk files?",
                      MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }
    m_engine.DiscardRecoveredDrafts();
    StartAsyncWorkspaceLoad(m_engine.ResourcePath());
}

void CMainFrame::OnAppExit() {
    PostMessage(WM_CLOSE);
}

void CMainFrame::OnClose() {
    if (m_engine.IsWorkspaceLoaded() && m_engine.Items().GetModifiedItemCount() > 0) {
        const int res = AfxMessageBox(
            L"You have unsaved modifications in this workspace.\r\n\r\n"
            L"Do you want to commit your changes to disk before exiting?\r\n"
            L"(Drafts will still be preserved in SQLite crash recovery if you choose No)",
            MB_YESNOCANCEL | MB_ICONQUESTION);
        if (res == IDCANCEL) return;
        if (res == IDYES) {
            std::string report;
            m_engine.CommitAllToDisk(report);
        }
    }
    CFrameWnd::OnClose();
}

void CMainFrame::OnDestroy() {
    KillTimer(kSearchDebounceTimer);
    if (m_hAccel) {
        ::DestroyAcceleratorTable(m_hAccel);
        m_hAccel = nullptr;
    }
    CFrameWnd::OnDestroy();
}

void CMainFrame::OnItemNew() {
    if (!m_engine.IsWorkspaceLoaded()) return;

    int defaultTable = m_cmbTableFilter.GetCurSel() - 1;
    if (defaultTable < 0) defaultTable = 0;

    CNewItemDlg dlg(m_engine, false, defaultTable, 0, "New Custom Item", this);
    if (dlg.DoModal() != IDOK) return;

    const int32_t newId = m_engine.CreateNewItem(
        dlg.SelectedTableIndex(),
        dlg.SelectedRawType(),
        dlg.SelectedEquipSubType(),
        dlg.SelectedName(),
        dlg.SelectedItemId()
    );

    if (newId <= 0) {
        AfxMessageBox(L"Failed to create new item. ID collision or invalid table schema.", MB_ICONERROR);
        return;
    }

    RunItemSearch(newId);
    SelectAndRevealItem(newId);
    UpdateStatusBar(std::format("Created new item #{} in table {}.", newId, m_engine.Items().GetItemTableName(dlg.SelectedTableIndex())));
}

void CMainFrame::OnItemDuplicate() {
    if (!m_engine.IsWorkspaceLoaded() || m_selectedItemId <= 0) return;
    const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
    if (!cur) return;

    const std::string defaultName = std::format("{} (Copy)", cur->name);
    CNewItemDlg dlg(m_engine, true, cur->tableIndex, cur->id, defaultName, this);
    if (dlg.DoModal() != IDOK) return;

    const int32_t newId = m_engine.DuplicateItem(
        cur->id,
        dlg.SelectedName(),
        dlg.SelectedTableIndex(),
        dlg.SelectedItemId()
    );

    if (newId <= 0) {
        AfxMessageBox(L"Failed to duplicate item. ID collision or invalid table schema.", MB_ICONERROR);
        return;
    }

    RunItemSearch(newId);
    SelectAndRevealItem(newId);
    UpdateStatusBar(std::format("Duplicated item #{} -> #{} ({}).", cur->id, newId, dlg.SelectedName()));
}

void CMainFrame::OnItemApply() {
    if (!m_engine.IsWorkspaceLoaded() || m_selectedItemId <= 0) return;
    const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
    if (!cur) return;

    Data::DNItem updated;
    std::string err;
    if (!CollectItemFromEditor(updated, err)) {
        AfxMessageBox(Utf8ToWide(err), MB_ICONWARNING);
        return;
    }

    if (ItemsEqualForAutoApply(*cur, updated)) {
        return;
    }

    if (m_engine.ApplyItemEdit(updated)) {
        Graphics::ImageBuffer tile;
        const bool hasIcon = m_engine.GetItemIcon(updated.iconIndex, tile);
        const size_t nameCollisions = (updated.nameId > 0 && m_engine.Items().CountItemsUsingMid(updated.nameId) > 1)
                                          ? (m_engine.Items().CountItemsUsingMid(updated.nameId) - 1)
                                          : 0;
        UpdateLiveSummaryPreview(
            updated.name,
            updated.rank,
            updated.iconIndex,
            hasIcon ? &tile : nullptr,
            nameCollisions
        );
        m_lstItems.Invalidate();
        UpdateWindowCaption();
        UpdateStatusBar(std::format("Applied edits to item #{} - {}", updated.id, updated.name));
    }
}

void CMainFrame::OnItemRevert() {
    if (m_selectedItemId <= 0) return;
    const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
    LoadItemIntoEditor(cur);
    UpdateStatusBar(std::format("Reverted editor controls for item #{}.", m_selectedItemId));
}

void CMainFrame::OnItemDelete() {
    if (!m_engine.IsWorkspaceLoaded() || m_selectedItemId <= 0) return;
    const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
    if (!cur) return;

    const CString prompt = Utf8ToWide(std::format("Are you sure you want to delete item row #{} '{}'?\nThis action can be undone with Ctrl+Z.", cur->id, cur->name));
    if (AfxMessageBox(prompt, MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    const int32_t deletedId = cur->id;
    if (m_engine.DeleteItem(deletedId)) {
        m_selectedItemId = 0;
        RunItemSearch();
        if (!m_filteredItems.empty()) {
            SelectAndRevealItem(m_filteredItems.front()->id);
        } else {
            LoadItemIntoEditor(nullptr);
        }
        UpdateWindowCaption();
        UpdateStatusBar(std::format("Deleted item row #{}.", deletedId));
    }
}

void CMainFrame::OnItemUndo() {
    if (!m_engine.CanUndo()) {
        UpdateStatusBar("Nothing to undo.");
        return;
    }
    std::string summary;
    if (m_engine.UndoLastAction(summary)) {
        RunItemSearch(m_selectedItemId);
        LoadItemIntoEditor(m_engine.Items().GetItem(m_selectedItemId));
        UpdateWindowCaption();
        UpdateStatusBar(summary);
    }
}

void CMainFrame::OnItemPickIcon() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    int32_t curIcon = ParseIntOr(m_edtIconIndex, 0);
    CIconPickerDlg dlg(m_engine, curIcon, this);
    if (dlg.DoModal() == IDOK) {
        const int32_t picked = dlg.SelectedIconIndex();
        m_edtIconIndex.SetWindowTextW(Utf8ToWide(std::to_string(picked)));
        Graphics::ImageBuffer tile;
        const bool hasIcon = m_engine.GetItemIcon(picked, tile);
        const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
        if (cur) {
            const size_t nameCollisions = (cur->nameId > 0 && m_engine.Items().CountItemsUsingMid(cur->nameId) > 1)
                                              ? (m_engine.Items().CountItemsUsingMid(cur->nameId) - 1)
                                              : 0;
            UpdateLiveSummaryPreview(
                cur->name,
                cur->rank,
                picked,
                hasIcon ? &tile : nullptr,
                nameCollisions
            );
            m_previewWnd.UpdateIconOnly(picked, cur->name, cur->rank, hasIcon ? &tile : nullptr);
        }
        if (m_chkAutoApply.GetCheck() == BST_CHECKED) {
            OnItemApply();
        }
        UpdateStatusBar(std::format("Selected icon index {}.", picked));
    }
}

void CMainFrame::OnFocusSearch() {
    m_edtSearch.SetFocus();
    m_edtSearch.SetSel(0, -1);
}

void CMainFrame::OnToolsVerifyRoundTrip() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    UpdateStatusBar("Verifying 1:1 round-trip binary fidelity across all loaded tables...");
    std::string report;
    const bool ok = m_engine.Items().VerifyRoundTrip(report);
    CReportDlg dlg(std::string(ok ? "1:1 Round-Trip Fidelity Verified [OK]" : "1:1 Round-Trip Verification Discrepancies [FAIL]"),
                   std::string(ok ? "Every loaded DNT table matches 1:1 with byte-identical serialisation."
                                  : "Discrepancies found between in-memory tables and original disk files."),
                   report, this);
    dlg.DoModal();
    UpdateStatusBar(ok ? "Round-trip verification PASSED." : "Round-trip verification FAILED.");
}

void CMainFrame::OnToolsWorkspaceAudit() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    UpdateStatusBar("Running workspace integrity audit across all items and detail tables...");
    const auto report = m_engine.Items().RunWorkspaceAudit();
    CReportDlg dlg("Workspace Integrity Audit Report",
                   std::format("Audit complete: {} items checked, {} issues flagged.", report.totalChecked,
                               report.emptyOrMissingNames + report.missingEquipDetailRows + report.invalidStatRanges),
                   report.reportText, this);
    dlg.DoModal();
    UpdateStatusBar("Workspace integrity audit completed.");
}

void CMainFrame::OnToolsClearCache() {
    m_engine.ClearNativeCaches();
    UpdateStatusBar("Native memory caches (icons, textures, meshes) cleared.");
}

void CMainFrame::OnFileSaveToFolder() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    const size_t modCount = m_engine.Items().GetModifiedItemCount();
    if (modCount == 0) {
        AfxMessageBox(L"No modified tables or items to save.", MB_ICONINFORMATION);
        return;
    }

    CFolderPickerDialog dlg(nullptr, OFN_FILEMUSTEXIST, this);
    dlg.m_ofn.lpstrTitle = L"Select Output Folder to Save Modified DNT Tables & UIStrings";
    if (dlg.DoModal() != IDOK) return;

    const std::filesystem::path outFolder(dlg.GetPathName().GetString());
    UpdateStatusBar("Saving modified tables and strings to folder...");
    std::string report;
    const bool ok = m_engine.SaveModifiedToFolder(outFolder, report);
    UpdateWindowCaption();
    UpdateStatusBar(ok ? "Saved modified files to folder successfully." : "Save encountered errors.");
    CReportDlg repDlg(std::string(ok ? "Save to Folder Successful [OK]" : "Save to Folder Warning [CHECK]"),
                      std::string(ok ? "Modified DNT tables and uistring.xml were written to the selected folder."
                                     : "Some files could not be saved. Check details below."),
                      report, this);
    repDlg.DoModal();
}

void CMainFrame::OnToolsCaptureSnapshot() {
    Common::Crash::CaptureSnapshot("Manual Snapshot from Tools Menu");
    UpdateStatusBar("Captured minidump and diagnostics snapshot to Logs folder.");
}

void CMainFrame::OnToolsOpenLogsFolder() {
    ::ShellExecuteW(nullptr, L"open", L"Logs", nullptr, nullptr, SW_SHOWNORMAL);
}

void CMainFrame::OnHelpAbout() {
    CAboutDlg dlg(this);
    dlg.DoModal();
}

void CMainFrame::OnRankComboChanged() {
    if (m_populatingEditor) return;
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) {
        OnItemApply();
    }
}

void CMainFrame::OnIconIndexEditChanged() {
    if (m_populatingEditor) return;
    const int32_t iconIdx = ParseIntOr(m_edtIconIndex, -1);
    if (iconIdx >= 0) {
        Graphics::ImageBuffer tile;
        const bool hasIcon = m_engine.GetItemIcon(iconIdx, tile);
        const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
        if (cur) {
            const size_t nameCollisions = (cur->nameId > 0 && m_engine.Items().CountItemsUsingMid(cur->nameId) > 1)
                                              ? (m_engine.Items().CountItemsUsingMid(cur->nameId) - 1)
                                              : 0;
            UpdateLiveSummaryPreview(
                cur->name,
                cur->rank,
                iconIdx,
                hasIcon ? &tile : nullptr,
                nameCollisions
            );
            m_previewWnd.UpdateIconOnly(iconIdx, cur->name, cur->rank, hasIcon ? &tile : nullptr);
        }
    }
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) {
        OnItemApply();
    }
}

void CMainFrame::OnJobsEditChanged() {
    if (m_populatingEditor || m_syncingClasses) return;
    const std::string jobs = GetEditUtf8(m_edtJobs);
    SyncClassCheckboxesFromJobString(jobs);
    m_lblJobsResolved.SetWindowTextW(Utf8ToWide(m_engine.Items().DescribeJobs(jobs)));
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) {
        OnItemApply();
    }
}

void CMainFrame::OnClassMatrixClicked(UINT nID) {
    if (m_populatingEditor) return;
    m_syncingClasses = true;

    if (nID == IDC_CHK_CLASS_ALL) {
        const bool checkAll = (m_chkClassAll.GetCheck() == BST_CHECKED);
        for (size_t i = 0; i < 10; ++i) {
            m_chkBaseClasses[i].SetCheck(checkAll ? BST_CHECKED : BST_UNCHECKED);
        }
        if (m_treeSubclasses.GetSafeHwnd()) {
            for (HTREEITEM h = m_treeSubclasses.GetRootItem(); h != nullptr; h = m_treeSubclasses.GetNextSiblingItem(h)) {
                m_treeSubclasses.SetCheck(h, checkAll ? TRUE : FALSE);
            }
        }
        const std::string newJobs = checkAll ? "1;2;3;4;5;6;7;8;9;201;" : "";
        m_edtJobs.SetWindowTextW(Utf8ToWide(newJobs));
        m_lblJobsResolved.SetWindowTextW(Utf8ToWide(m_engine.Items().DescribeJobs(newJobs)));
    } else {
        const size_t bIdx = static_cast<size_t>(nID - IDC_CHK_CLASS_BASE_FIRST);
        if (bIdx < 10 && m_treeSubclasses.GetSafeHwnd()) {
            const int32_t bId = kBaseClassIds[bIdx];
            const BOOL isChecked = (m_chkBaseClasses[bIdx].GetCheck() == BST_CHECKED);
            for (HTREEITEM h = m_treeSubclasses.GetRootItem(); h != nullptr; h = m_treeSubclasses.GetNextSiblingItem(h)) {
                if (static_cast<int32_t>(m_treeSubclasses.GetItemData(h)) == bId) {
                    m_treeSubclasses.SetCheck(h, isChecked);
                    break;
                }
            }
        }

        std::vector<int32_t> ids;
        CollectCheckedJobIds(ids);
        std::string newJobs;
        for (int32_t id : ids) {
            newJobs += std::format("{};", id);
        }

        bool allChecked = true;
        for (size_t i = 0; i < 10; ++i) {
            if (m_chkBaseClasses[i].GetCheck() != BST_CHECKED) {
                allChecked = false;
                break;
            }
        }
        m_chkClassAll.SetCheck(allChecked ? BST_CHECKED : BST_UNCHECKED);
        m_edtJobs.SetWindowTextW(Utf8ToWide(newJobs));
        m_lblJobsResolved.SetWindowTextW(Utf8ToWide(m_engine.Items().DescribeJobs(newJobs)));
    }

    m_syncingClasses = false;
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) {
        OnItemApply();
    }
}

void CMainFrame::OnSubclassTreeClick(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    *pResult = 0;
    if (m_populatingEditor || m_syncingClasses) return;

    DWORD dwPos = ::GetMessagePos();
    CPoint pt(GET_X_LPARAM(dwPos), GET_Y_LPARAM(dwPos));
    m_treeSubclasses.ScreenToClient(&pt);

    UINT uFlags = 0;
    HTREEITEM hItem = m_treeSubclasses.HitTest(pt, &uFlags);
    if (hItem && (uFlags & TVHT_ONITEMSTATEICON)) {
        const BOOL bNew = !m_treeSubclasses.GetCheck(hItem);
        m_treeSubclasses.SetCheck(hItem, bNew);
        *pResult = 1; // suppress default tree toggle to avoid double-state bounce
        OnSubclassTreeCheckChanged(hItem, bNew);
    }
}

void CMainFrame::OnSubclassTreeKeyDown(NMHDR* pNMHDR, LRESULT* pResult) {
    *pResult = 0;
    if (m_populatingEditor || m_syncingClasses) return;

    auto* pTvKey = reinterpret_cast<NMTVKEYDOWN*>(pNMHDR);
    if (pTvKey && pTvKey->wVKey == VK_SPACE) {
        HTREEITEM hItem = m_treeSubclasses.GetSelectedItem();
        if (hItem) {
            const BOOL bNew = !m_treeSubclasses.GetCheck(hItem);
            m_treeSubclasses.SetCheck(hItem, bNew);
            *pResult = 1;
            OnSubclassTreeCheckChanged(hItem, bNew);
        }
    }
}

void CMainFrame::OnSubclassTreeRClick(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    *pResult = 0;
    DWORD dwPos = ::GetMessagePos();
    CPoint pt(GET_X_LPARAM(dwPos), GET_Y_LPARAM(dwPos));
    CPoint ptClient = pt;
    m_treeSubclasses.ScreenToClient(&ptClient);

    UINT uFlags = 0;
    HTREEITEM hItem = m_treeSubclasses.HitTest(ptClient, &uFlags);
    if (!hItem) return;

    m_treeSubclasses.SelectItem(hItem);

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenuW(MF_STRING, ID_CMD_SUBCLASS_CHECK_BRANCH, L"Check Entire Branch");
    menu.AppendMenuW(MF_STRING, ID_CMD_SUBCLASS_UNCHECK_BRANCH, L"Uncheck Entire Branch");
    menu.AppendMenuW(MF_SEPARATOR);
    menu.AppendMenuW(MF_STRING, ID_CMD_SUBCLASS_TOGGLE_EXPAND, L"Expand / Collapse Branch");

    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    *pResult = 1;
}

void CMainFrame::OnSubclassTreeCheckChanged(HTREEITEM hItem, BOOL bChecked) {
    if (m_populatingEditor || m_syncingClasses) return;
    m_syncingClasses = true;

    if (hItem) {
        const auto jid = static_cast<int32_t>(m_treeSubclasses.GetItemData(hItem));
        for (size_t i = 0; i < 10; ++i) {
            if (kBaseClassIds[i] == jid) {
                m_chkBaseClasses[i].SetCheck(bChecked ? BST_CHECKED : BST_UNCHECKED);
                break;
            }
        }
    }

    std::vector<int32_t> ids;
    CollectCheckedJobIds(ids);

    std::string newJobs;
    for (int32_t id : ids) {
        newJobs += std::format("{};", id);
    }

    bool all10 = true;
    for (size_t i = 0; i < 10; ++i) {
        if (m_chkBaseClasses[i].GetCheck() != BST_CHECKED) {
            all10 = false;
            break;
        }
    }
    m_chkClassAll.SetCheck(all10 ? BST_CHECKED : BST_UNCHECKED);

    m_edtJobs.SetWindowTextW(Utf8ToWide(newJobs));
    m_lblJobsResolved.SetWindowTextW(Utf8ToWide(m_engine.Items().DescribeJobs(newJobs)));

    m_syncingClasses = false;

    if (m_chkAutoApply.GetCheck() == BST_CHECKED) {
        OnItemApply();
    }
}

void CMainFrame::OnSubclassCheckBranch() {
    HTREEITEM hItem = m_treeSubclasses.GetSelectedItem();
    if (!hItem) return;
    SetBranchChecked(hItem, TRUE);
    OnSubclassTreeCheckChanged(hItem, TRUE);
}

void CMainFrame::OnSubclassUncheckBranch() {
    HTREEITEM hItem = m_treeSubclasses.GetSelectedItem();
    if (!hItem) return;
    SetBranchChecked(hItem, FALSE);
    OnSubclassTreeCheckChanged(hItem, FALSE);
}

void CMainFrame::OnSubclassAll1st() {
    if (m_populatingEditor) return;
    m_syncingClasses = true;

    auto checkTier1 = [&](auto& self, HTREEITEM hItem) -> void {
        while (hItem) {
            const auto jid = static_cast<int32_t>(m_treeSubclasses.GetItemData(hItem));
            for (const auto& entry : kDNClassHierarchy) {
                if (entry.jobId == jid && entry.tier == 1) {
                    m_treeSubclasses.SetCheck(hItem, TRUE);
                    break;
                }
            }
            HTREEITEM hChild = m_treeSubclasses.GetChildItem(hItem);
            if (hChild) self(self, hChild);
            hItem = m_treeSubclasses.GetNextSiblingItem(hItem);
        }
    };
    checkTier1(checkTier1, m_treeSubclasses.GetRootItem());

    std::vector<int32_t> ids;
    CollectCheckedJobIds(ids);
    std::string newJobs;
    for (int32_t id : ids) newJobs += std::format("{};", id);

    m_edtJobs.SetWindowTextW(Utf8ToWide(newJobs));
    m_lblJobsResolved.SetWindowTextW(Utf8ToWide(m_engine.Items().DescribeJobs(newJobs)));
    m_syncingClasses = false;

    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
}

void CMainFrame::OnSubclassAll2nd() {
    if (m_populatingEditor) return;
    m_syncingClasses = true;

    auto checkTier2 = [&](auto& self, HTREEITEM hItem) -> void {
        while (hItem) {
            const auto jid = static_cast<int32_t>(m_treeSubclasses.GetItemData(hItem));
            for (const auto& entry : kDNClassHierarchy) {
                if (entry.jobId == jid && entry.tier == 2) {
                    m_treeSubclasses.SetCheck(hItem, TRUE);
                    break;
                }
            }
            HTREEITEM hChild = m_treeSubclasses.GetChildItem(hItem);
            if (hChild) self(self, hChild);
            hItem = m_treeSubclasses.GetNextSiblingItem(hItem);
        }
    };
    checkTier2(checkTier2, m_treeSubclasses.GetRootItem());

    std::vector<int32_t> ids;
    CollectCheckedJobIds(ids);
    std::string newJobs;
    for (int32_t id : ids) newJobs += std::format("{};", id);

    m_edtJobs.SetWindowTextW(Utf8ToWide(newJobs));
    m_lblJobsResolved.SetWindowTextW(Utf8ToWide(m_engine.Items().DescribeJobs(newJobs)));
    m_syncingClasses = false;

    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
}

void CMainFrame::OnSubclassClear() {
    if (m_populatingEditor) return;
    m_syncingClasses = true;

    auto uncheckAll = [&](auto& self, HTREEITEM hItem) -> void {
        while (hItem) {
            m_treeSubclasses.SetCheck(hItem, FALSE);
            HTREEITEM hChild = m_treeSubclasses.GetChildItem(hItem);
            if (hChild) self(self, hChild);
            hItem = m_treeSubclasses.GetNextSiblingItem(hItem);
        }
    };
    uncheckAll(uncheckAll, m_treeSubclasses.GetRootItem());

    std::vector<int32_t> ids;
    CollectCheckedJobIds(ids);
    std::string newJobs;
    for (int32_t id : ids) newJobs += std::format("{};", id);

    m_edtJobs.SetWindowTextW(Utf8ToWide(newJobs));
    m_lblJobsResolved.SetWindowTextW(Utf8ToWide(m_engine.Items().DescribeJobs(newJobs)));
    m_syncingClasses = false;

    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
}

void CMainFrame::OnSubclassToggleExpand() {
    m_subclassesExpanded = !m_subclassesExpanded;
    const UINT code = m_subclassesExpanded ? TVE_EXPAND : TVE_COLLAPSE;

    for (HTREEITEM h = m_treeSubclasses.GetRootItem(); h != nullptr; h = m_treeSubclasses.GetNextSiblingItem(h)) {
        m_treeSubclasses.Expand(h, code);
    }

    m_btnSubclassExpand.SetWindowTextW(m_subclassesExpanded ? L"Collapse" : L"Expand");
}

void CMainFrame::OnAutoCalcSell() {
    MutateCurrentItem([](Data::DNItem& item) {
        item.sellAmount = Data::ItemManager::ComputeSuggestedSellAmount(item);
    });
    if (const auto* cur = m_engine.Items().GetItem(m_selectedItemId)) {
        m_edtSell.SetWindowTextW(Utf8ToWide(std::to_string(cur->sellAmount)));
    }
}

void CMainFrame::OnAutoCalcGearScore() {
    MutateCurrentItem([](Data::DNItem& item) {
        item.gearScore = Data::ItemManager::ComputeSuggestedGearScore(item);
    });
    if (const auto* cur = m_engine.Items().GetItem(m_selectedItemId)) {
        m_edtGearScore.SetWindowTextW(Utf8ToWide(std::to_string(cur->gearScore)));
    }
}

void CMainFrame::OnAutoCalcDurability() {
    MutateCurrentItem([](Data::DNItem& item) {
        item.durability = Data::ItemManager::ComputeSuggestedDurability(item);
    });
    if (const auto* cur = m_engine.Items().GetItem(m_selectedItemId)) {
        m_edtDurability.SetWindowTextW(Utf8ToWide(std::to_string(cur->durability)));
    }
}

void CMainFrame::OnAutoCalcAllEconomy() {
    Data::DNItem item; std::string err;
    if (!CollectItemFromEditor(item, err)) return;
    m_edtSell.SetWindowTextW(Utf8ToWide(std::to_string(Data::ItemManager::ComputeSuggestedSellAmount(item))));
    m_edtGearScore.SetWindowTextW(Utf8ToWide(std::to_string(Data::ItemManager::ComputeSuggestedGearScore(item))));
    m_edtDurability.SetWindowTextW(Utf8ToWide(std::to_string(Data::ItemManager::ComputeSuggestedDurability(item))));
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar("Auto-calculated Sell Gold, Gear Score, and Durability.");
}

void CMainFrame::OnPresetEquipDefaults() {
    m_edtOverlap.SetWindowTextW(L"1");
    m_chkCash.SetCheck(BST_UNCHECKED);
    m_chkStorage.SetCheck(BST_CHECKED);
    m_chkWStorage.SetCheck(BST_CHECKED);
    OnAutoCalcAllEconomy();
    UpdateStatusBar("Applied Standard Equipment Presets (Overlap 1, Storage Enabled).");
}

void CMainFrame::OnPresetStack999() {
    m_edtOverlap.SetWindowTextW(L"999");
    m_edtDurability.SetWindowTextW(L"0");
    m_edtGearScore.SetWindowTextW(L"0");
    m_chkCash.SetCheck(BST_UNCHECKED);
    m_chkStorage.SetCheck(BST_CHECKED);
    m_chkWStorage.SetCheck(BST_CHECKED);
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar("Applied Stackable Material Preset (Overlap 999).");
}

void CMainFrame::OnPresetCashBound() {
    m_chkCash.SetCheck(BST_CHECKED);
    m_chkStorage.SetCheck(BST_UNCHECKED);
    m_chkWStorage.SetCheck(BST_UNCHECKED);
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar("Applied Cash / Character-Bound Preset.");
}

void CMainFrame::OnLiveTextPreviewChanged() {
    if (m_populatingEditor) return;
    UpdateLiveResolvedPreviews();
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) {
        OnItemApply();
    }
}

void CMainFrame::OnAllocNameMid() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    const uint32_t newMid = m_engine.Items().AllocateUIStringMid("New UI String");
    m_edtNameMid.SetWindowTextW(Utf8ToWide(std::to_string(newMid)));
    UpdateMidCollisionBanner();
    UpdateLiveResolvedPreviews();
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar(std::format("Allocated fresh exclusive Name MID: {}", newMid));
}

void CMainFrame::OnAllocDescMid() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    const uint32_t newMid = m_engine.Items().AllocateUIStringMid("New UI String");
    m_edtDescMid.SetWindowTextW(Utf8ToWide(std::to_string(newMid)));
    UpdateMidCollisionBanner();
    UpdateLiveResolvedPreviews();
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar(std::format("Allocated fresh exclusive Desc MID: {}", newMid));
}

void CMainFrame::OnForkExclusiveMids() {
    if (!m_engine.IsWorkspaceLoaded() || m_selectedItemId <= 0) return;
    if (m_engine.ForkExclusiveMids(m_selectedItemId)) {
        LoadItemIntoEditor(m_engine.Items().GetItem(m_selectedItemId));
        UpdateMidCollisionBanner();
        UpdateStatusBar(std::format("Forked exclusive Name/Desc MIDs for item #{}.", m_selectedItemId));
    }
}

void CMainFrame::OnSearchNameUIString() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    const std::string initQuery = GetEditUtf8(m_edtNameResolved);
    CUIStringSearchDlg dlg(m_engine, initQuery, this);
    if (dlg.DoModal() == IDOK) {
        m_edtNameMid.SetWindowTextW(Utf8ToWide(std::to_string(dlg.SelectedMid())));
        m_edtNameTmpl.SetWindowTextW(Utf8ToWide(dlg.SelectedText()));
        UpdateLiveResolvedPreviews();
        UpdateMidCollisionBanner();
        if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    }
}

void CMainFrame::OnSearchDescUIString() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    const std::string initQuery = GetEditUtf8(m_edtDescResolved);
    CUIStringSearchDlg dlg(m_engine, initQuery, this);
    if (dlg.DoModal() == IDOK) {
        m_edtDescMid.SetWindowTextW(Utf8ToWide(std::to_string(dlg.SelectedMid())));
        m_edtDescTmpl.SetWindowTextW(Utf8ToWide(dlg.SelectedText()));
        UpdateLiveResolvedPreviews();
        UpdateMidCollisionBanner();
        if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    }
}

void CMainFrame::OnGenerateRichTooltip() {
    Data::DNItem item; std::string err;
    if (!CollectItemFromEditor(item, err)) return;
    const std::string rich = m_engine.Items().GenerateRichTooltipDescription(item);
    m_edtDescTmpl.SetWindowTextW(Utf8ToWide(rich));
    UpdateLiveResolvedPreviews();
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar("Generated rich formatted tooltip description.");
}

void CMainFrame::OnInsertColorTag(UINT nID) {
    constexpr const wchar_t* kTags[7] = {
        L"#y", L"#r", L"#b", L"#g", L"#v", L"#w", L"\\n"
    };
    const size_t idx = nID - IDC_BTN_COLOR_FIRST;
    if (idx < 7) {
        m_edtDescTmpl.ReplaceSel(kTags[idx], TRUE);
        m_edtDescTmpl.SetFocus();
        UpdateLiveResolvedPreviews();
        if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    }
}

void CMainFrame::OnStatsApplyArchetype() {
    Data::DNItem item; std::string err;
    if (!CollectItemFromEditor(item, err)) return;
    const auto arch = static_cast<Data::StatArchetype>(std::clamp(m_cmbArchetype.GetCurSel(), 0, 5));
    Data::ItemManager::ApplyStatArchetype(item, arch);
    PopulateStatWidgetsFromItem(item);
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar("Applied stat archetype preset.");
}

void CMainFrame::OnStatsAutoCurve() {
    Data::DNItem item; std::string err;
    if (!CollectItemFromEditor(item, err)) return;
    Data::ItemManager::AutoScaleStatsByLevelAndRank(item);
    PopulateStatWidgetsFromItem(item);
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar("Auto-scaled stats by level and rank curve.");
}

void CMainFrame::OnStatsScaleButton(UINT nID) {
    constexpr double kFactors[5] = { 1.10, 1.25, 1.50, 2.00, 0.80 };
    const size_t idx = nID - IDC_BTN_STAT_SCALE_FIRST;
    if (idx >= 5) return;
    Data::DNItem item; std::string err;
    if (!CollectItemFromEditor(item, err)) return;
    Data::ItemManager::ScaleItemStats(item, kFactors[idx]);
    PopulateStatWidgetsFromItem(item);
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar(std::format("Scaled stats by factor {:.2f}.", kFactors[idx]));
}

void CMainFrame::OnStatsSpread15() {
    Data::DNItem item; std::string err;
    if (!CollectItemFromEditor(item, err)) return;
    for (auto& s : item.states) {
        if (s.type > 0) {
            const double mn = std::strtod(s.minValue.c_str(), nullptr);
            if (mn > 0.0) {
                const double mx = mn * 1.15;
                if ((s.type >= 16 && s.type <= 23) || (s.type >= 50 && s.type <= 81)) {
                    s.maxValue = std::format("{:.4f}", mx);
                } else {
                    s.maxValue = std::format("{:.0f}", std::round(mx));
                }
            }
        }
    }
    PopulateStatWidgetsFromItem(item);
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar("Spread Max values to Min + 15%.");
}

void CMainFrame::OnStatsCopyMinToMax() {
    Data::DNItem item; std::string err;
    if (!CollectItemFromEditor(item, err)) return;
    for (auto& s : item.states) {
        if (s.type > 0) {
            s.maxValue = s.minValue;
        }
    }
    PopulateStatWidgetsFromItem(item);
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar("Set Max = Min across all active stat slots.");
}

void CMainFrame::OnStatsProbsFull() {
    Data::DNItem item; std::string err;
    if (!CollectItemFromEditor(item, err)) return;
    for (auto& s : item.states) {
        if (s.type > 0) {
            s.genProb = 1.0f;
        }
    }
    PopulateStatWidgetsFromItem(item);
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar("Set GenProb = 1.0 (100%) across all active stat slots.");
}

void CMainFrame::OnStatsCompact() {
    Data::DNItem item; std::string err;
    if (!CollectItemFromEditor(item, err)) return;
    Data::ItemManager::CompactAndSanitizeStats(item);
    PopulateStatWidgetsFromItem(item);
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar("Compacted stat slots to contiguous list.");
}

void CMainFrame::OnStatsCopyClipboard() {
    Data::DNItem item; std::string err;
    if (!CollectItemFromEditor(item, err)) return;
    m_statClipboard = item.states;
    m_hasStatClipboard = true;
    UpdateStatusBar("Copied 10 stat slots to internal clipboard.");
}

void CMainFrame::OnStatsPasteClipboard() {
    if (!m_hasStatClipboard) {
        UpdateStatusBar("Stat clipboard is empty. Click 'Copy Stats' first.");
        return;
    }
    Data::DNItem item; std::string err;
    if (!CollectItemFromEditor(item, err)) return;
    item.states = m_statClipboard;
    PopulateStatWidgetsFromItem(item);
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar("Pasted 10 stat slots from internal clipboard.");
}

void CMainFrame::OnStatsClearUnused() {
    for (size_t i = 0; i < Data::kStateSlotCount; ++i) {
        auto& row = m_statWidgets[i];
        row.cmbType.SetCurSel(0);
        row.preservedUnknownType = -1;
        row.edtProb.SetWindowTextW(L"0.0000");
        row.edtMin.SetWindowTextW(L"0");
        row.edtMax.SetWindowTextW(L"0");
    }
    if (m_chkAutoApply.GetCheck() == BST_CHECKED) OnItemApply();
    UpdateStatusBar("Cleared all 10 stat slots.");
}

void CMainFrame::OnRenderModeChanged() {
    const int sel = m_cmbRenderMode.GetCurSel();
    Graphics::RenderMode mode = Graphics::RenderMode::TexturedLit;
    if (sel == 1) mode = Graphics::RenderMode::ClayShaded;
    else if (sel == 2) mode = Graphics::RenderMode::Wireframe;
    m_previewWnd.SetRenderMode(mode);
}

void CMainFrame::OnToggleAutoSpin() {
    m_previewWnd.SetAutoSpin(m_chkAutoSpin.GetCheck() == BST_CHECKED);
}

void CMainFrame::OnZoomIn3D() {
    m_previewWnd.AdjustZoom(1.2f);
}

void CMainFrame::OnZoomOut3D() {
    m_previewWnd.AdjustZoom(1.0f / 1.2f);
}

void CMainFrame::OnResetCamera() {
    m_previewWnd.ResetCamera();
}

void CMainFrame::OnUseSelectedAsDonor() {
    if (m_selectedItemId <= 0) return;
    m_visualDonorItemId = m_selectedItemId;
    m_edtDonorId.SetWindowTextW(Utf8ToWide(std::to_string(m_visualDonorItemId)));
    UpdateStatusBar(std::format("Set donor item to #{}.", m_visualDonorItemId));
}

void CMainFrame::CopyVisualHelper(bool copySkin, bool copyIcon, const char* label) {
    const int32_t donor = ParseIntOr(m_edtDonorId, m_visualDonorItemId);
    if (donor <= 0 || m_selectedItemId <= 0) {
        UpdateStatusBar("Please specify a valid donor item ID first.");
        return;
    }
    if (m_engine.CopyVisualFromItem(m_selectedItemId, donor, copyIcon, copySkin)) {
        LoadItemIntoEditor(m_engine.Items().GetItem(m_selectedItemId));
        UpdateStatusBar(std::format("Copied {} from donor #{} onto #{}.", label, donor, m_selectedItemId));
    }
}

void CMainFrame::OnCopyVisualBoth() {
    CopyVisualHelper(true, true, "3D skin and 2D icon");
}

void CMainFrame::OnCopyVisualSkinOnly() {
    CopyVisualHelper(true, false, "3D skin");
}

void CMainFrame::OnCopyVisualIconOnly() {
    CopyVisualHelper(false, true, "2D icon index");
}

void CMainFrame::OnRawTableSourceChanged() {
    PopulateRawInspectorList();
}

void CMainFrame::OnRawFilterChanged() {
    PopulateRawInspectorList();
}

void CMainFrame::OnRawListSelectionChanged(NMHDR* pNMHDR, LRESULT* pResult) {
    auto* nmlv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    *pResult = 0;
    if ((nmlv->uChanged & LVIF_STATE) && (nmlv->uNewState & LVIS_SELECTED)) {
        if (nmlv->iItem >= 0 && static_cast<size_t>(nmlv->iItem) < m_rawEntries.size()) {
            const auto& e = m_rawEntries[static_cast<size_t>(nmlv->iItem)];
            m_selectedRawColIdx = static_cast<int>(e.colIdx);
            m_lblRawSelectedCol.SetWindowTextW(Utf8ToWide(std::format("[Col {}] {}", e.colIdx, e.name)));
            m_edtRawCellValue.SetWindowTextW(Utf8ToWide(e.value));
        }
    }
}

void CMainFrame::OnRawListDblClick(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    *pResult = 0;
    m_edtRawCellValue.SetFocus();
    m_edtRawCellValue.SetSel(0, -1);
}

void CMainFrame::OnRawWriteCell() {
    if (m_selectedRawColIdx < 0 || m_selectedItemId <= 0) return;
    const bool detailTable = (m_cmbRawSource.GetCurSel() == 1);
    const std::string newText = GetEditUtf8(m_edtRawCellValue);
    if (m_engine.SetRawItemCell(m_selectedItemId, detailTable, static_cast<size_t>(m_selectedRawColIdx), newText)) {
        LoadItemIntoEditor(m_engine.Items().GetItem(m_selectedItemId));
        PopulateRawInspectorList();
        UpdateStatusBar(std::format("Wrote raw cell [col {}] = '{}'.", m_selectedRawColIdx, newText));
    } else {
        AfxMessageBox(L"Failed to write raw cell value. Check format and data type.", MB_ICONERROR);
    }
}

void CMainFrame::OnRawCopyRowTsv() {
    const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
    if (!cur) return;
    const bool detailTable = (m_cmbRawSource.GetCurSel() == 1);
    const Data::DNTTable* tbl = nullptr;
    if (!detailTable) {
        if (cur->tableIndex < m_engine.Items().GetItemTableCount()) {
            tbl = &m_engine.Items().GetItemTable(cur->tableIndex);
        }
    } else {
        tbl = m_engine.Items().GetDetailTableForItem(*cur);
    }
    if (!tbl) return;
    const auto* row = tbl->GetRow(cur->id);
    if (!row) return;

    std::string tsvHeader, tsvData;
    for (size_t c = 0; c < tbl->GetColumns().size(); ++c) {
        if (c > 0) { tsvHeader += '\t'; tsvData += '\t'; }
        tsvHeader += tbl->GetColumns()[c].name;
        tsvData += tbl->FormatCell(*row, c);
    }
    const std::string full = tsvHeader + "\r\n" + tsvData;
    CopyUtf8ToClipboard(GetSafeHwnd(), full);
    UpdateStatusBar("Copied raw DNT row TSV to clipboard.");
}

void CMainFrame::OnRawCloneDetailFromDonor() {
    const int32_t donor = ParseIntOr(m_edtDonorId, m_visualDonorItemId);
    if (donor <= 0 || m_selectedItemId <= 0) {
        UpdateStatusBar("Please specify a valid donor item ID first.");
        return;
    }
    if (m_engine.CloneDetailRowFromItem(m_selectedItemId, donor)) {
        LoadItemIntoEditor(m_engine.Items().GetItem(m_selectedItemId));
        PopulateRawInspectorList();
        UpdateStatusBar(std::format("Cloned weapon/parts detail row from donor #{} onto #{}.", donor, m_selectedItemId));
    }
}

void CMainFrame::OnGenerateTierSet() {
    if (!m_engine.IsWorkspaceLoaded() || m_selectedItemId <= 0) return;
    Engine::TierSetConfig cfg;
    cfg.sourceItemId = m_selectedItemId;
    cfg.baseName = GetEditUtf8(m_edtTierBaseName);
    if (cfg.baseName.empty()) {
        const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
        if (cur) cfg.baseName = cur->name;
    }
    for (size_t i = 0; i < 7; ++i) {
        cfg.includeRanks[i] = (m_chkTierRanks[i].GetCheck() == BST_CHECKED);
    }
    cfg.statGrowthPerRankPct = ParseDoubleOr(m_edtTierGrowth, 28.0);
    cfg.useAutoCurve = (m_chkTierAutoCurve.GetCheck() == BST_CHECKED);
    cfg.autoRecalcEconomy = (m_chkTierAutoEcon.GetCheck() == BST_CHECKED);
    cfg.autoRichTooltip = (m_chkTierRichDesc.GetCheck() == BST_CHECKED);

    std::string summary;
    const auto createdIds = m_engine.GenerateRankTierSet(cfg, summary);
    if (createdIds.empty()) {
        AfxMessageBox(Utf8ToWide(std::format("Tier set generation failed:\n{}", summary)), MB_ICONERROR);
        return;
    }
    RunItemSearch(createdIds.front());
    SelectAndRevealItem(createdIds.front());
    ShowActiveEditorTab(0);
    UpdateStatusBar(std::format("Generated {} tier items.", createdIds.size()));
    CReportDlg dlg("Tier Set Generation Summary", "Multi-Rank Tier Set generated successfully.", summary, this);
    dlg.DoModal();
}

void CMainFrame::OnGenerateClassSeries() {
    if (!m_engine.IsWorkspaceLoaded() || m_selectedItemId <= 0) return;
    Engine::ClassSeriesConfig cfg;
    cfg.sourceItemId = m_selectedItemId;
    cfg.seriesNameTemplate = GetEditUtf8(m_edtSeriesTmpl);
    if (cfg.seriesNameTemplate.empty()) {
        const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
        cfg.seriesNameTemplate = cur ? std::format("{{CLASS}} {}", cur->name) : "{CLASS} Equipment";
    }
    for (size_t i = 0; i < 10; ++i) {
        cfg.includeClasses[i] = (m_chkSeriesClasses[i].GetCheck() == BST_CHECKED);
    }
    cfg.adaptPhysicalMagicalStats = (m_chkSeriesAdaptStats.GetCheck() == BST_CHECKED);
    cfg.autoRichTooltip = (m_chkSeriesRichDesc.GetCheck() == BST_CHECKED);

    std::string summary;
    const auto createdIds = m_engine.GenerateClassEquipmentSeries(cfg, summary);
    if (createdIds.empty()) {
        AfxMessageBox(Utf8ToWide(std::format("Class series generation failed:\n{}", summary)), MB_ICONERROR);
        return;
    }
    RunItemSearch(createdIds.front());
    SelectAndRevealItem(createdIds.front());
    ShowActiveEditorTab(0);
    UpdateStatusBar(std::format("Generated {} class series items.", createdIds.size()));
    CReportDlg dlg("Class Series Generation Summary", "Multi-Class Equipment Series generated successfully.", summary, this);
    dlg.DoModal();
}

void CMainFrame::OnExecuteBatchEdit() {
    if (!m_engine.IsWorkspaceLoaded() || m_filteredItems.empty()) {
        AfxMessageBox(L"No items match the current browser filter.", MB_ICONINFORMATION);
        return;
    }

    std::vector<int32_t> itemIds;
    itemIds.reserve(m_filteredItems.size());
    for (const auto* it : m_filteredItems) itemIds.push_back(it->id);

    Engine::BatchEditSpec spec;
    spec.scaleStats = (m_chkBatchScaleStats.GetCheck() == BST_CHECKED);
    spec.statMultiplier = ParseDoubleOr(m_edtBatchScaleFactor, 1.0);
    spec.autoCurveStats = (m_chkBatchAutoCurve.GetCheck() == BST_CHECKED);
    spec.sanitizeStats = (m_chkBatchSanitize.GetCheck() == BST_CHECKED);
    spec.autoRecalcEconomy = (m_chkBatchAutoEcon.GetCheck() == BST_CHECKED);
    spec.setRank = (m_chkBatchSetRank.GetCheck() == BST_CHECKED);
    spec.newRank = m_cmbBatchRank.GetCurSel();
    spec.setLevelLimit = (m_chkBatchSetLevel.GetCheck() == BST_CHECKED);
    spec.newLevelLimit = ParseIntOr(m_edtBatchLevel, 1);
    spec.setOverlapCount = (m_chkBatchSetOverlap.GetCheck() == BST_CHECKED);
    spec.newOverlapCount = ParseIntOr(m_edtBatchOverlap, 1);
    spec.namePrefix = GetEditUtf8(m_edtBatchPrefix);
    spec.nameSuffix = GetEditUtf8(m_edtBatchSuffix);

    const CString prompt = Utf8ToWide(std::format(
        "Apply batch operations across all {} currently filtered items?\r\n"
        "This action can be undone with Ctrl+Z.", itemIds.size()));
    if (AfxMessageBox(prompt, MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    std::string summary;
    const size_t modified = m_engine.ExecuteBatchEdit(itemIds, spec, summary);
    RunItemSearch(m_selectedItemId);
    LoadItemIntoEditor(m_engine.Items().GetItem(m_selectedItemId));
    UpdateStatusBar(std::format("Batch edit modified {} items.", modified));
    CReportDlg dlg("Batch Mass-Edit Summary", std::format("Batch modified {} items.", modified), summary, this);
    dlg.DoModal();
}

void CMainFrame::OnExportSelectedJson() {
    if (m_selectedItemId <= 0) return;
    ExportItemPackInteractive({ m_selectedItemId }, std::format("item_{}.json", m_selectedItemId));
}

void CMainFrame::OnExportModifiedJson() {
    const auto ids = m_engine.Items().GetModifiedItemIds();
    if (ids.empty()) {
        AfxMessageBox(L"No modified items to export.", MB_ICONINFORMATION);
        return;
    }
    ExportItemPackInteractive(ids, "modified_items_pack.json");
}

void CMainFrame::OnExportFilteredJson() {
    if (m_filteredItems.empty()) {
        AfxMessageBox(L"No items match the current browser filter.", MB_ICONINFORMATION);
        return;
    }
    std::vector<int32_t> ids;
    ids.reserve(m_filteredItems.size());
    for (const auto* it : m_filteredItems) ids.push_back(it->id);
    ExportItemPackInteractive(ids, "filtered_items_pack.json");
}

void CMainFrame::OnImportJsonFile() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    std::filesystem::path pickedFile;
    if (!PickOpenFileDialog(GetSafeHwnd(), "json", pickedFile)) return;

    std::ifstream ifs(pickedFile, std::ios::binary);
    if (!ifs) {
        AfxMessageBox(L"Failed to open selected JSON file.", MB_ICONERROR);
        return;
    }
    std::string jsonText((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    int targetTable = m_cmbTableFilter.GetCurSel() - 1;
    if (targetTable < 0) targetTable = -1;

    std::vector<int32_t> createdIds;
    std::string summary;
    const size_t imported = m_engine.ImportItemsJson(jsonText, targetTable, createdIds, summary);
    if (imported == 0) {
        AfxMessageBox(Utf8ToWide(std::format("JSON import failed:\n{}", summary)), MB_ICONERROR);
        return;
    }
    RunItemSearch(createdIds.front());
    SelectAndRevealItem(createdIds.front());
    ShowActiveEditorTab(0);
    UpdateStatusBar(std::format("Imported {} items from JSON.", imported));
    CReportDlg dlg("JSON Item Pack Import Summary", std::format("Successfully imported {} items.", imported), summary, this);
    dlg.DoModal();
}

void CMainFrame::OnImportJsonClipboard() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    const std::string jsonText = ReadUtf8FromClipboard(GetSafeHwnd());
    if (jsonText.empty()) {
        AfxMessageBox(L"Clipboard does not contain valid text.", MB_ICONINFORMATION);
        return;
    }

    int targetTable = m_cmbTableFilter.GetCurSel() - 1;
    if (targetTable < 0) targetTable = -1;

    std::vector<int32_t> createdIds;
    std::string summary;
    const size_t imported = m_engine.ImportItemsJson(jsonText, targetTable, createdIds, summary);
    if (imported == 0) {
        AfxMessageBox(Utf8ToWide(std::format("JSON clipboard import failed:\n{}", summary)), MB_ICONERROR);
        return;
    }
    RunItemSearch(createdIds.front());
    SelectAndRevealItem(createdIds.front());
    ShowActiveEditorTab(0);
    UpdateStatusBar(std::format("Imported {} items from clipboard JSON.", imported));
    CReportDlg dlg("JSON Clipboard Import Summary", std::format("Successfully imported {} items.", imported), summary, this);
    dlg.DoModal();
}

void CMainFrame::OnRefreshModifiedList() {
    RefreshModifiedItemsTabList();
}

void CMainFrame::OnModifiedListDblClick(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    *pResult = 0;
    const int sel = m_lstModifiedItems.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0 || static_cast<size_t>(sel) >= m_modifiedListIds.size()) return;
    const int32_t targetId = m_modifiedListIds[static_cast<size_t>(sel)];
    SelectAndRevealItem(targetId);
    ShowActiveEditorTab(0);
}


void CMainFrame::LoadInitialWorkspaceWithSplash(const std::filesystem::path& resourcePath, CSplashWnd* splash) {
    m_loadingWorkspace.store(true);
    m_pendingPath = resourcePath;
    m_filteredItems.clear();
    m_lstItems.SetItemCountEx(0, LVSICF_NOINVALIDATEALL);

    size_t recovered = 0;
    HWND hwnd = GetSafeHwnd();
    const bool ok = m_engine.OpenWorkspace(resourcePath, &recovered, [this, hwnd]() {
        if (::IsWindow(hwnd)) {
            ::PostMessageW(hwnd, kMsgModelIndexReady, 0, 0);
        }
    }, [splash](float p, std::string_view stage, std::string_view detail) {
        if (splash) {
            splash->SetProgress(p, stage, detail);
        }
    });

    m_loadingWorkspace.store(false);
    if (ok) {
        SetWorkspaceControlsEnabled(true);
        PopulateSubclassesTree();
        RunItemSearch();
        if (!m_filteredItems.empty()) {
            SelectAndRevealItem(m_filteredItems[0]->id);
        }
        UpdateWindowCaption();
        UpdateStatusBar(std::format("Workspace loaded: {} items in memory ({} recovered drafts).",
                                    m_engine.Items().GetItemCount(), recovered));
    } else {
        SetWorkspaceControlsEnabled(false);
        UpdateStatusBar("Failed to load workspace folder.");
    }
}

// ---------------------------------------------------------------------------
// Charm & Gacha Box Probability Studio Methods
// ---------------------------------------------------------------------------

void CMainFrame::PopulateCharmTabForCurrentItem() {
    if (!m_cmbCharmSelector.GetSafeHwnd()) return;

    m_cmbCharmSelector.ResetContent();
    const auto& charmNums = m_engine.Items().GetAllCharmNums();
    if (charmNums.empty()) {
        m_cmbCharmSelector.AddString(L"No Charm/Gacha Tables Loaded");
        m_cmbCharmSelector.SetCurSel(0);
        m_lstCharmRewards.DeleteAllItems();
        m_lblCharmStatSummary.SetWindowTextW(L"No charm or gacha tables available in this workspace.");
        m_lblCharmStatOdds.SetWindowTextW(L"");
        return;
    }

    int32_t activeNum = 0;
    if (m_selectedItemId > 0) {
        if (const auto* itm = m_engine.Items().GetItem(m_selectedItemId)) {
            activeNum = m_engine.Items().GetItemCharmNum(*itm);
        }
    }
    if (activeNum == 0 && m_currentCharmNum > 0) {
        activeNum = m_currentCharmNum;
    }
    if (activeNum == 0 && !charmNums.empty()) {
        activeNum = charmNums.front();
    }

    int selIdx = 0;
    for (size_t i = 0; i < charmNums.size(); ++i) {
        const int32_t cNum = charmNums[i];
        const auto* grp = m_engine.Items().GetCharmGroup(cNum);
        std::wstring label;
        if (grp) {
            label = std::format(L"Drop Set #{:05d} ({} rewards) [{}]", cNum, grp->rewards.size(), Utf8ToWide(grp->sourceTable).GetString());
        } else {
            label = std::format(L"Drop Set #{:05d}", cNum);
        }
        const int idx = m_cmbCharmSelector.AddString(label.c_str());
        m_cmbCharmSelector.SetItemData(idx, static_cast<DWORD_PTR>(cNum));
        if (cNum == activeNum) {
            selIdx = idx;
        }
    }

    m_cmbCharmSelector.SetCurSel(selIdx);
    PopulateCharmRewardsList(activeNum);
}

void CMainFrame::PopulateCharmRewardsList(int32_t charmNum) {
    m_currentCharmNum = charmNum;
    m_selectedCharmRowId = 0;
    m_lstCharmRewards.DeleteAllItems();

    const auto* grp = m_engine.Items().GetCharmGroup(charmNum);
    if (!grp) {
        m_lblCharmStatSummary.SetWindowTextW(L"Drop pool not found.");
        m_lblCharmStatOdds.SetWindowTextW(L"");
        return;
    }

    m_lstCharmRewards.SetRedraw(FALSE);
    for (size_t i = 0; i < grp->rewards.size(); ++i) {
        const auto& rew = grp->rewards[i];
        const int row = static_cast<int>(i);

        m_lstCharmRewards.InsertItem(row, Utf8ToWide(std::to_string(i + 1)));
        m_lstCharmRewards.SetItemText(row, 1, Utf8ToWide(std::to_string(rew.rowId)));
        m_lstCharmRewards.SetItemText(row, 2, Utf8ToWide(std::to_string(rew.itemId)));
        m_lstCharmRewards.SetItemText(row, 3, Utf8ToWide(rew.itemName));

        std::wstring rankStr;
        switch (rew.itemRank) {
            case 0: rankStr = L"Normal (D)"; break;
            case 1: rankStr = L"Magic (C)"; break;
            case 2: rankStr = L"Rare (B)"; break;
            case 3: rankStr = L"Epic (A)"; break;
            case 4: rankStr = L"Unique (S)"; break;
            case 5: rankStr = L"Legend (SS)"; break;
            case 6: rankStr = L"Named (SSS)"; break;
            default: rankStr = std::format(L"Rank {}", rew.itemRank); break;
        }
        m_lstCharmRewards.SetItemText(row, 4, rankStr.c_str());
        m_lstCharmRewards.SetItemText(row, 5, Utf8ToWide(std::to_string(rew.count)));
        m_lstCharmRewards.SetItemText(row, 6, Utf8ToWide(std::to_string(rew.probWeight)));
        m_lstCharmRewards.SetItemText(row, 7, std::format(L"{:.4f}%", rew.dropRatePercent).c_str());

        std::wstring oddsStr = L"-";
        if (rew.dropRatePercent > 0.0) {
            const double oneInN = 100.0 / rew.dropRatePercent;
            if (oneInN >= 1000.0) {
                oddsStr = std::format(L"1 in {:L}", static_cast<int64_t>(std::round(oneInN)));
            } else if (oneInN >= 10.0) {
                oddsStr = std::format(L"1 in {:.1f}", oneInN);
            } else {
                oddsStr = std::format(L"1 in {:.2f}", oneInN);
            }
        }
        m_lstCharmRewards.SetItemText(row, 8, oddsStr.c_str());
        m_lstCharmRewards.SetItemText(row, 9, Utf8ToWide(std::to_string(rew.potenProb)));
        m_lstCharmRewards.SetItemText(row, 10, rew.broadcast ? L"YES" : L"No");
        m_lstCharmRewards.SetItemText(row, 11, Utf8ToWide(std::to_string(rew.gold)));
        m_lstCharmRewards.SetItemData(row, static_cast<DWORD_PTR>(rew.rowId));
    }
    m_lstCharmRewards.SetRedraw(TRUE);
    m_lstCharmRewards.Invalidate();

    UpdateCharmStatsAndAnalytics(*grp);
}

void CMainFrame::UpdateCharmStatsAndAnalytics(const Data::CharmGroup& grp) {
    if (grp.rewards.empty()) {
        m_lblCharmStatSummary.SetWindowTextW(L"Empty drop set.");
        m_lblCharmStatOdds.SetWindowTextW(L"");
        return;
    }

    size_t jackpotCount = 0;
    double jackpotTotalRate = 0.0;
    double minRate = 100.0;
    std::string rarestName;

    for (const auto& r : grp.rewards) {
        if (r.broadcast || r.itemRank >= 4) {
            jackpotCount++;
            jackpotTotalRate += r.dropRatePercent;
        }
        if (r.probWeight > 0 && r.dropRatePercent < minRate) {
            minRate = r.dropRatePercent;
            rarestName = r.itemName;
        }
    }

    const std::wstring summary = std::format(
        L"Drop Pool #{:05d}  |  Source: {}  |  Total Entries: {}  |  Total Weight Sum: {:L}\r\n"
        L"Jackpot Items: {} ({:.4f}% combined rate)  |  Rarest Drop: '{}' ({:.4f}%)",
        grp.charmNum,
        Utf8ToWide(grp.sourceTable).GetString(),
        grp.rewards.size(),
        grp.totalWeight,
        jackpotCount,
        jackpotTotalRate,
        Utf8ToWide(rarestName).GetString(),
        minRate
    );
    m_lblCharmStatSummary.SetWindowTextW(summary.c_str());

    std::wstring odds;
    if (minRate > 0.0 && minRate < 100.0) {
        const double P = minRate / 100.0;
        const double expPulls = 1.0 / P;
        const double pLog = std::log(1.0 - P);
        const double n50 = std::ceil(std::log(0.5) / pLog);
        const double n90 = std::ceil(std::log(0.10) / pLog);
        const double n99 = std::ceil(std::log(0.01) / pLog);

        odds = std::format(
            L"Statistical Pity Line Expectations for Rarest Item ('{}' @ {:.4f}%):\r\n"
            L" - Mean Expected Pulls: {:L} pulls\r\n"
            L" - 50% Median (Coin flip chance): {:L} pulls\r\n"
            L" - 90% Confidence (9 in 10 players): {:L} pulls\r\n"
            L" - 99% Soft Pity (99 in 100 players): {:L} pulls",
            Utf8ToWide(rarestName).GetString(), minRate,
            static_cast<int64_t>(std::round(expPulls)),
            static_cast<int64_t>(n50),
            static_cast<int64_t>(n90),
            static_cast<int64_t>(n99)
        );
    } else {
        odds = L"Statistical Pity Line: Not applicable for uniform or zero drop weights.";
    }
    m_lblCharmStatOdds.SetWindowTextW(odds.c_str());
}

void CMainFrame::RunGachaSimulation(int pullCount, bool untilJackpot) {
    const auto* grp = m_engine.Items().GetCharmGroup(m_currentCharmNum);
    if (!grp || grp->rewards.empty() || grp->totalWeight <= 0) {
        AfxMessageBox(L"Cannot run simulation on an empty drop set or a set with zero total weight.", MB_ICONWARNING);
        return;
    }

    std::vector<int64_t> cumWeights;
    cumWeights.reserve(grp->rewards.size());
    int64_t running = 0;
    for (const auto& rew : grp->rewards) {
        running += std::max<int32_t>(0, rew.probWeight);
        cumWeights.push_back(running);
    }
    if (running <= 0) return;

    std::random_device rd;
    std::mt19937_64 rng(rd());
    std::uniform_int_distribution<int64_t> dist(1, running);

    const int maxIterations = untilJackpot ? 100000 : pullCount;
    int pullsDone = 0;
    bool hitJackpot = false;

    for (int i = 0; i < maxIterations; ++i) {
        pullsDone++;
        m_simState.totalPulls++;

        const int64_t roll = dist(rng);
        const auto it = std::lower_bound(cumWeights.begin(), cumWeights.end(), roll);
        const size_t idx = std::distance(cumWeights.begin(), it);
        const auto& rew = grp->rewards[idx];

        m_simState.itemHits[rew.itemId]++;
        m_simState.totalGoldWon += rew.gold;

        const bool isJackpot = rew.broadcast || rew.itemRank >= 4;
        if (isJackpot) {
            m_simState.jackpotsHit++;
            hitJackpot = true;
        }

        std::string record = std::format("[Pull #{}] {} x{} (Rank {}, {:.4f}%)",
                                         m_simState.totalPulls, rew.itemName, rew.count, rew.itemRank, rew.dropRatePercent);
        if (isJackpot) {
            record += " *** JACKPOT! ***";
        }
        m_simState.recentPulls.push_front(record);
        if (m_simState.recentPulls.size() > 40) {
            m_simState.recentPulls.pop_back();
        }

        if (untilJackpot && hitJackpot) {
            break;
        }
    }

    UpdateGachaSimDisplay();
}

void CMainFrame::UpdateGachaSimDisplay() {
    std::wostringstream oss;
    oss << L"=== Gacha Simulation Results ===\r\n";
    oss << std::format(L"Total Pulls Executed: {:L}\r\n", m_simState.totalPulls);
    oss << std::format(L"Total Jackpots Won: {:L}\r\n", m_simState.jackpotsHit);
    if (m_simState.totalPulls > 0) {
        const double jackRate = (static_cast<double>(m_simState.jackpotsHit) / static_cast<double>(m_simState.totalPulls)) * 100.0;
        oss << std::format(L"Empirical Jackpot Rate: {:.4f}%\r\n", jackRate);
    }
    oss << std::format(L"Total Gold Accumulated: {:L} gold\r\n", m_simState.totalGoldWon);
    oss << L"\r\n--- Recent Pull Log ---\r\n";
    for (const auto& logEntry : m_simState.recentPulls) {
        oss << Utf8ToWide(logEntry).GetString() << L"\r\n";
    }

    m_edtSimLog.SetWindowTextW(oss.str().c_str());
    m_edtSimLog.LineScroll(m_edtSimLog.GetLineCount());
}

void CMainFrame::OnSimReset() {
    m_simState = GachaSimState{};
    UpdateGachaSimDisplay();
}

void CMainFrame::OnToolsCharmCalculator() {
    if (std::find(m_visibleTabs.begin(), m_visibleTabs.end(), 7) == m_visibleTabs.end()) {
        m_visibleTabs.push_back(7);
        std::sort(m_visibleTabs.begin(), m_visibleTabs.end());
        RebuildTabControl();
    }
    ShowActiveEditorTab(7);
}

void CMainFrame::OnCharmSelectorChange() {
    const int sel = m_cmbCharmSelector.GetCurSel();
    if (sel < 0) return;
    const auto cNum = static_cast<int32_t>(m_cmbCharmSelector.GetItemData(sel));
    PopulateCharmRewardsList(cNum);
}

void CMainFrame::OnCharmSyncCurrentItem() {
    if (m_selectedItemId <= 0) return;
    const auto* itm = m_engine.Items().GetItem(m_selectedItemId);
    if (!itm) return;
    const int32_t cNum = m_engine.Items().GetItemCharmNum(*itm);
    if (cNum <= 0) {
        AfxMessageBox(L"Current item is not linked to any charm or gacha drop pool.", MB_ICONINFORMATION);
        return;
    }
    for (int i = 0; i < m_cmbCharmSelector.GetCount(); ++i) {
        if (static_cast<int32_t>(m_cmbCharmSelector.GetItemData(i)) == cNum) {
            m_cmbCharmSelector.SetCurSel(i);
            PopulateCharmRewardsList(cNum);
            return;
        }
    }
}

void CMainFrame::OnCharmRefresh() {
    PopulateCharmTabForCurrentItem();
}

void CMainFrame::OnCharmRewardItemSelect(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    if (pResult) *pResult = 0;
    const int sel = m_lstCharmRewards.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0) {
        m_selectedCharmRowId = 0;
        m_edtCharmEditWeight.SetWindowTextW(L"");
        return;
    }
    m_selectedCharmRowId = static_cast<int32_t>(m_lstCharmRewards.GetItemData(sel));
    CString wText = m_lstCharmRewards.GetItemText(sel, 6);
    m_edtCharmEditWeight.SetWindowTextW(wText);
}

void CMainFrame::OnCharmNormalizeWeights() {
    if (m_currentCharmNum <= 0) return;
    CString tText;
    m_edtCharmTargetWeight.GetWindowTextW(tText);
    const int64_t targetTotal = _wtoi64(tText);
    if (targetTotal <= 0) {
        AfxMessageBox(L"Please enter a valid positive target total weight (e.g., 100000).", MB_ICONWARNING);
        return;
    }
    if (m_engine.Items().NormalizeCharmGroupWeights(m_currentCharmNum, targetTotal)) {
        PopulateCharmRewardsList(m_currentCharmNum);
        UpdateStatusBar(std::format("Normalized Charm Group #{} weights to {:L}.", m_currentCharmNum, targetTotal));
    }
}

void CMainFrame::OnCharmApplySelectedWeight() {
    if (m_selectedCharmRowId <= 0 || m_currentCharmNum <= 0) {
        AfxMessageBox(L"Please select a reward row from the table first.", MB_ICONINFORMATION);
        return;
    }
    CString wText;
    m_edtCharmEditWeight.GetWindowTextW(wText);
    const int32_t newWeight = _wtoi(wText);
    if (newWeight < 0) {
        AfxMessageBox(L"Weight must be zero or positive.", MB_ICONWARNING);
        return;
    }
    if (m_engine.Items().SetCharmItemProb(m_selectedCharmRowId, newWeight)) {
        PopulateCharmRewardsList(m_currentCharmNum);
        UpdateStatusBar(std::format("Updated Charm Reward Row #{} weight to {:L}.", m_selectedCharmRowId, newWeight));
    }
}

void CMainFrame::OnCharmExportCsv() {
    const auto* grp = m_engine.Items().GetCharmGroup(m_currentCharmNum);
    if (!grp || grp->rewards.empty()) {
        AfxMessageBox(L"No drop rewards to export.", MB_ICONINFORMATION);
        return;
    }

    std::filesystem::path pickedPath;
    if (!PickSaveFileDialog(GetSafeHwnd(), std::format("charm_group_{}.csv", m_currentCharmNum), "csv", pickedPath)) {
        return;
    }

    std::ofstream ofs(pickedPath);
    if (!ofs) {
        AfxMessageBox(L"Failed to create CSV file for export.", MB_ICONERROR);
        return;
    }

    ofs << "#,RowID,ItemID,RewardName,Rank,Count,Weight,DropPercent,Odds,PotenProb,Broadcast,Gold\n";
    for (size_t i = 0; i < grp->rewards.size(); ++i) {
        const auto& rew = grp->rewards[i];
        double oneInN = (rew.dropRatePercent > 0.0) ? (100.0 / rew.dropRatePercent) : 0.0;
        ofs << (i + 1) << ","
            << rew.rowId << ","
            << rew.itemId << ","
            << "\"" << rew.itemName << "\","
            << rew.itemRank << ","
            << rew.count << ","
            << rew.probWeight << ","
            << std::fixed << std::setprecision(4) << rew.dropRatePercent << "%,"
            << "1 in " << std::fixed << std::setprecision(1) << oneInN << ","
            << rew.potenProb << ","
            << (rew.broadcast ? "1" : "0") << ","
            << rew.gold << "\n";
    }
    ofs.close();

    UpdateStatusBar(std::format("Exported {} rewards for Charm Group #{} to '{}'.",
                                grp->rewards.size(), m_currentCharmNum, pickedPath.filename().string()));
    AfxMessageBox(L"CSV export completed successfully!", MB_ICONINFORMATION);
}




void CMainFrame::OnToolsDragonJewelStudio() {
    CDragonJewelStudioDlg dlg(m_engine, m_selectedItemId, this);
    if (dlg.DoModal() == IDOK) {
        if (dlg.SelectedItemId() > 0 && dlg.SelectedItemId() != m_selectedItemId) {
            SelectAndRevealItem(dlg.SelectedItemId());
        } else if (m_selectedItemId > 0) {
            LoadItemIntoEditor(m_engine.Items().GetItem(m_selectedItemId));
        }
    }
}

void CMainFrame::OnToolsSetItemStudio() {
    int32_t initialSetId = 0;
    if (m_selectedItemId > 0) {
        if (const auto* itm = m_engine.Items().GetItem(m_selectedItemId)) {
            initialSetId = itm->setItemId;
        }
    }
    CSetItemStudioDlg dlg(m_engine, initialSetId, this);
    if (dlg.DoModal() == IDOK) {
        if (dlg.JumpToItemId() > 0) {
            SelectAndRevealItem(dlg.JumpToItemId());
        }
    }
}

void CMainFrame::OnToolsEnchantStudio() {
    int32_t encId = 0;
    if (m_selectedItemId > 0) {
        if (const auto* itm = m_engine.Items().GetItem(m_selectedItemId)) {
            encId = itm->enchantId;
        }
    }
    CEnchantStudioDlg dlg(m_engine, encId, m_selectedItemId, this);
    dlg.DoModal();
}


void CMainFrame::OnCharClassChanged() {
    const int sel = m_cmbCharClass.GetCurSel();
    if (sel < 0) return;
    const auto cls = static_cast<Engine::CharacterClassId>(sel);
    m_previewWnd.SetCharacterClass(cls);

    m_cmbCharAction.ResetContent();
    const auto acts = m_previewWnd.GetAvailableActions();
    for (const auto& a : acts) {
        m_cmbCharAction.AddString(Utf8ToWide(a));
    }
    if (m_cmbCharAction.GetCount() > 0) {
        m_cmbCharAction.SetCurSel(0);
        CString text;
        m_cmbCharAction.GetLBText(0, text);
        m_previewWnd.SetAction(WideToUtf8(text.GetString()));
    }
}

void CMainFrame::OnCharActionChanged() {
    const int sel = m_cmbCharAction.GetCurSel();
    if (sel < 0) return;
    CString text;
    m_cmbCharAction.GetLBText(sel, text);
    m_previewWnd.SetAction(WideToUtf8(text.GetString()));
}

void CMainFrame::OnAnimPlayPauseClicked() {
    const bool nowPlaying = !m_previewWnd.IsPlaying();
    m_previewWnd.SetPlaying(nowPlaying);
    m_btnAnimPlayPause.SetWindowTextW(nowPlaying ? L"⏸ Pause" : L"▶ Play");
}

void CMainFrame::OnAnimResetClicked() {
    m_previewWnd.ResetAnimPlayback();
}

void CMainFrame::OnAnimLoopToggled() {
    m_previewWnd.SetLoop(m_chkAnimLoop.GetCheck() == BST_CHECKED);
}

void CMainFrame::OnAnimSpeedChanged() {
    const int sel = m_cmbAnimSpeed.GetCurSel();
    float spd = 1.0f;
    switch (sel) {
        case 0: spd = 0.25f; break;
        case 1: spd = 0.5f; break;
        case 2: spd = 1.0f; break;
        case 3: spd = 1.5f; break;
        case 4: spd = 2.0f; break;
        default: spd = 1.0f; break;
    }
    m_previewWnd.SetSpeed(spd);
}


void CMainFrame::OnOpenIconEditor() {
    if (!m_engine.IsWorkspaceLoaded()) return;
    int32_t curIcon = ParseIntOr(m_edtIconIndex, 0);
    CIconEditorDlg dlg(m_engine, m_selectedItemId, curIcon, this);
    if (dlg.DoModal() == IDOK) {
        Graphics::ImageBuffer tile;
        const bool hasIcon = m_engine.GetItemIcon(curIcon, tile);
        const auto* cur = m_engine.Items().GetItem(m_selectedItemId);
        if (cur) {
            const size_t nameCollisions = (cur->nameId > 0 && m_engine.Items().CountItemsUsingMid(cur->nameId) > 1)
                                              ? (m_engine.Items().CountItemsUsingMid(cur->nameId) - 1)
                                              : 0;
            UpdateLiveSummaryPreview(
                cur->name,
                cur->rank,
                curIcon,
                hasIcon ? &tile : nullptr,
                nameCollisions
            );
            m_previewWnd.UpdateIconOnly(curIcon, cur->name, cur->rank, hasIcon ? &tile : nullptr);
        }
        if (m_chkAutoApply.GetCheck() == BST_CHECKED) {
            OnItemApply();
        }
    }
}

void CMainFrame::OnOpenActConverter() {
    CActConverterDlg dlg(m_engine, this);
    dlg.DoModal();
}

} // namespace DNItemMaker::UI

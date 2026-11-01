#include "ItemManager.h"



#include "Common/Logs/Logs.h"



#ifndef WIN32_LEAN_AND_MEAN

#define WIN32_LEAN_AND_MEAN

#endif

#ifndef NOMINMAX

#define NOMINMAX

#endif

#include <windows.h>



#include <algorithm>

#include <charconv>

#include <chrono>

#include <cmath>

#include <cstring>

#include <format>

#include <fstream>

#include <thread>



namespace DNItemMaker::Data {

std::string DragonJewelInfo::GetTypeName() const {
    switch (type) {
        case 1: return "Offensive / Attack Jade";
        case 2: return "Defensive / Armor Jade";
        case 3: return "Skill / Special Jade";
        default: return std::format("Type {}", type);
    }
}

std::string DragonJewelInfo::GetElementName() const {
    switch (elementalType) {
        case 0: return "Water / Ice";
        case 1: return "Fire";
        case 2: return "Light";
        case 3: return "Dark";
        case -1: return "None (Physical)";
        default: return std::format("Element {}", elementalType);
    }
}

std::vector<std::string> DragonJewelInfo::GetApplicableSlotNames() const {
    std::vector<std::string> names;
    if (applicableSlots & (1 << 0)) names.push_back("Main Weapon");
    if (applicableSlots & (1 << 1)) names.push_back("Secondary");
    if (applicableSlots & (1 << 2)) names.push_back("Helmet");
    if (applicableSlots & (1 << 3)) names.push_back("Upper Body");
    if (applicableSlots & (1 << 4)) names.push_back("Lower Body");
    if (applicableSlots & (1 << 5)) names.push_back("Gloves");
    if (applicableSlots & (1 << 6)) names.push_back("Shoes");
    if (applicableSlots & (1 << 7)) names.push_back("Necklace");
    if (applicableSlots & (1 << 8)) names.push_back("Earrings");
    if (applicableSlots & (1 << 9)) names.push_back("Rings");
    if (names.empty()) {
        names.push_back(std::format("0x{:08X}", static_cast<uint32_t>(applicableSlots)));
    }
    return names;
}

int DragonJewelSlotConfig::GetActiveSlotCount() const noexcept {
    int cnt = 0;
    for (int s : slots) {
        if (s > 0) ++cnt;
    }
    return cnt;
}

std::string DragonJewelSlotConfig::GetSummary() const {
    int atk = 0, def = 0, skl = 0;
    for (int s : slots) {
        if (s == 1) ++atk;
        else if (s == 2) ++def;
        else if (s == 3) ++skl;
    }
    std::string s;
    if (atk > 0) s += std::format("{}x Attack ", atk);
    if (def > 0) s += std::format("{}x Defense ", def);
    if (skl > 0) s += std::format("{}x Skill ", skl);
    if (s.empty()) s = "No Sockets";
    else {
        while (!s.empty() && s.back() == ' ') s.pop_back();
    }
    return s;
}

std::string SetItemBonus::GetFormattedBonus() const {
    const char* name = ItemManager::StateDisplayName(stateType);
    return std::format("{} +{}", name, stateValue);
}




namespace {



const std::string kEmptyString;



struct StateTypeInfo {

    int32_t type;

    const char* name;

};



// Verified against CDnItem::CalcStateValue in the client source.

constexpr StateTypeInfo kStateTypes[] = {

    { 0, "Strength" }, { 1, "Agility" }, { 2, "Intelligence" }, { 3, "Vitality" },

    { 4, "Physical Attack Min" }, { 5, "Physical Attack Max" }, { 6, "Magic Attack Min" }, { 7, "Magic Attack Max" },

    { 8, "Physical Defense" }, { 9, "Magic Defense" }, { 10, "Stiff" }, { 11, "Stiff Resistance" },

    { 12, "Critical" }, { 13, "Critical Resistance" }, { 14, "Stun" }, { 15, "Stun Resistance" },

    { 16, "Fire Attack" }, { 17, "Ice Attack" }, { 18, "Light Attack" }, { 19, "Dark Attack" },

    { 20, "Fire Defense" }, { 21, "Ice Defense" }, { 22, "Light Defense" }, { 23, "Dark Defense" },

    { 24, "Move Speed" }, { 25, "Max HP" }, { 26, "Max SP" }, { 27, "SP Recovery" }, { 28, "Super Armor" },

    { 29, "Final Damage" }, { 30, "Safe Zone Move Speed" }, { 31, "EXP Bonus" },

    { 32, "Physical Attack (Min+Max)" }, { 33, "Magic Attack (Min+Max)" },

    { 50, "Strength %" }, { 51, "Agility %" }, { 52, "Intelligence %" }, { 53, "Vitality %" },

    { 54, "Physical Attack Min %" }, { 55, "Physical Attack Max %" }, { 56, "Magic Attack Min %" }, { 57, "Magic Attack Max %" },

    { 58, "Physical Defense %" }, { 59, "Magic Defense %" }, { 60, "Stiff %" }, { 61, "Stiff Resistance %" },

    { 62, "Critical %" }, { 63, "Critical Resistance %" }, { 64, "Stun %" }, { 65, "Stun Resistance %" },

    { 66, "Fire Attack %" }, { 67, "Ice Attack %" }, { 68, "Light Attack %" }, { 69, "Dark Attack %" },

    { 70, "Fire Defense %" }, { 71, "Ice Defense %" }, { 72, "Light Defense %" }, { 73, "Dark Defense %" },

    { 74, "Move Speed %" }, { 75, "Max HP %" }, { 76, "Max SP %" }, { 77, "SP Recovery %" }, { 78, "Super Armor %" },

    { 79, "Final Damage %" }, { 81, "Safe Zone Move Speed %" }, { 100, "Spirit" },

};



struct TypeNameInfo {

    int32_t type;

    const char* name;

};



// Subset of eItemTypeEnum (Common/Protocol/DNItemTypeDef.h) that appears in the shipped item tables.

constexpr TypeNameInfo kTypeNames[] = {

    { 0, "Weapon" }, { 1, "Armor / Parts" }, { 2, "General" }, { 3, "Skill" }, { 4, "Jewel" }, { 5, "Heraldry / Plate" },

    { 6, "Instant" }, { 7, "Quest" }, { 8, "Random Box" }, { 9, "Hair Dye" }, { 10, "Skill Book" },

    { 11, "Enchant Success Up" }, { 12, "Enchant Break Guard" }, { 13, "Revive Coin" }, { 14, "Inventory Slot" },

    { 15, "Warehouse Slot" }, { 27, "Seal" }, { 38, "Glyph" }, { 43, "Potential Jewel" }, { 46, "Charm" },

    { 50, "Secondary Skill Recipe" }, { 51, "Appellation" }, { 65, "Cooking" }, { 97, "Sealable Normal" },

    { 105, "Party Buff" }, { 112, "All-Give Charm" }, { 113, "Choice Cube" }, { 122, "Random Charm" },

    { 123, "Direct Party Buff" }, { 128, "Ring" }, { 132, "Talisman" }, { 139, "Dragon Gem" },

    { 140, "Dragon Gem Remover" },

};



// Branchless, locale-independent ASCII lowercase (enables SIMD vectorization, zero CRT locale locks).

constexpr char AsciiLower(char c) noexcept {

    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;

}



std::string ToLower(std::string_view s) {

    std::string out(s.size(), '\0');

    const char* src = s.data();

    char* dst = out.data();

    for (size_t i = 0, n = s.size(); i < n; ++i) {

        dst[i] = AsciiLower(src[i]);

    }

    return out;

}



bool EqualsIgnoreCaseAscii(std::string_view a, std::string_view b) noexcept {

    if (a.size() != b.size()) return false;

    for (size_t i = 0; i < a.size(); ++i) {

        if (AsciiLower(a[i]) != AsciiLower(b[i])) return false;

    }

    return true;

}



bool ContainsSubstringFast(std::string_view haystack, std::string_view needle) noexcept {

    if (needle.empty()) return true;

    const size_t nLen = needle.size();

    if (haystack.size() < nLen) return false;

    const char first = needle.front();

    const char* cur = haystack.data();

    const char* const end = cur + haystack.size();

    while (static_cast<size_t>(end - cur) >= nLen) {

        const auto* hit = static_cast<const char*>(

            std::memchr(cur, static_cast<unsigned char>(first), static_cast<size_t>(end - cur) - nLen + 1));

        if (!hit) return false;

        if (nLen == 1 || std::memcmp(hit + 1, needle.data() + 1, nLen - 1) == 0) return true;

        cur = hit + 1;

    }

    return false;

}



constexpr std::string_view Trim(std::string_view sv) noexcept {

    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n')) sv.remove_prefix(1);

    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '\n')) sv.remove_suffix(1);

    return sv;

}



template <typename Fn>

void ParallelFor(size_t count, Fn&& fn, size_t chunkSize = 1, size_t maxWorkers = 12) {

    if (count == 0) return;

    const size_t step = std::max<size_t>(1, chunkSize);

    const size_t totalChunks = (count + step - 1) / step;

    const size_t hw = std::max<size_t>(1, std::thread::hardware_concurrency());

    const size_t workers = std::min({ totalChunks, hw, maxWorkers });

    if (workers <= 1) {

        for (size_t i = 0; i < count; ++i) fn(i);

        return;

    }



    std::atomic<size_t> nextBase{ 0 };

    std::atomic<size_t> completedWorkers{ 0 };

    auto workerLoop = [&]() {

        while (true) {

            const size_t begin = nextBase.fetch_add(step, std::memory_order_relaxed);

            if (begin >= count) break;

            const size_t end = std::min(count, begin + step);

            for (size_t i = begin; i < end; ++i) {

                fn(i);

            }

        }

        completedWorkers.fetch_add(1, std::memory_order_release);

    };



    std::vector<std::jthread> pool;

    pool.reserve(workers);

    for (size_t w = 0; w < workers; ++w) {

        pool.emplace_back(workerLoop);

    }



    MSG msg;

    while (completedWorkers.load(std::memory_order_acquire) < workers) {

        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {

            ::TranslateMessage(&msg);

            ::DispatchMessageW(&msg);

        }

        ::Sleep(1);

    }

}



constexpr ItemGroup DeriveGroup(int32_t rawType, int32_t sub) noexcept {

    if (rawType == 0) return ItemGroup::Weapon;

    if (rawType == 128) return ItemGroup::Accessory;

    if (rawType == 1) return (sub >= 7 && sub <= 9) ? ItemGroup::Accessory : ItemGroup::Armor;

    return ItemGroup::Other;

}



constexpr const char* WeaponTypeName(int32_t t) noexcept {

    constexpr const char* names[] = {

        "Sword", "Gauntlet", "Axe", "Hammer", "Short Bow", "Long Bow", "Crossbow", "Staff",

        "Book", "Orb", "Puppet", "Mace", "Flail", "Wand", "Shield", "Arrow",

        "Cannon", "Bubble Gun", "Chain", "Fan", "Chakram", "Charm", "Scimitar", "Dagger",

        "Sickle", "Spear", "(26)", "Bracelet"

    };

    return (t >= 0 && t < static_cast<int32_t>(std::size(names))) ? names[t] : "Weapon";

}



constexpr const char* PartsTypeName(int32_t p) noexcept {

    switch (p) {

        case 0: return "Face";

        case 1: return "Hair";

        case 2: return "Helmet";

        case 3: return "Body";

        case 4: return "Legs";

        case 5: return "Gloves";

        case 6: return "Boots";

        case 7: return "Necklace";

        case 8: return "Earrings";

        case 9: return "Ring";

        default: return "Armor";

    }

}



std::vector<std::filesystem::path> DiscoverSplitTables(const std::filesystem::path& extDir, std::string_view prefix) {

    struct Candidate {

        std::filesystem::path path;

        std::string lowerName;

        int rank = 2;

    };



    std::vector<Candidate> found;

    std::error_code ec;

    const std::string primary = std::format("{}.dnt", prefix);

    const std::string equip = std::format("{}_equipment.dnt", prefix);

    const std::string prefixUnderscore = std::format("{}_", prefix);



    for (const auto& entry : std::filesystem::directory_iterator(extDir, ec)) {

        if (!entry.is_regular_file(ec)) continue;

        const auto u8 = entry.path().filename().u8string();

        std::string lower = ToLower(std::string_view(reinterpret_cast<const char*>(u8.data()), u8.size()));

        if (!lower.ends_with(".dnt")) continue;

        if (lower == primary || lower.starts_with(prefixUnderscore)) {

            const int rank = (lower == primary) ? 0 : ((lower == equip) ? 1 : 2);

            found.push_back(Candidate{ entry.path(), std::move(lower), rank });

        }

    }



    std::sort(found.begin(), found.end(), [](const Candidate& a, const Candidate& b) {

        if (a.rank != b.rank) return a.rank < b.rank;

        return a.lowerName < b.lowerName;

    });



    std::vector<std::filesystem::path> files;

    files.reserve(found.size());

    for (auto& c : found) {

        files.push_back(std::move(c.path));

    }

    return files;

}



std::string ExtractTableSuffix(std::string_view fileName, std::string_view prefix) {

    std::string lower = ToLower(fileName);

    if (lower.ends_with(".dnt")) lower.resize(lower.size() - 4);

    if (lower == prefix) return {};

    if (lower.starts_with(prefix) && lower.size() > prefix.size() && lower[prefix.size()] == '_') {

        return lower.substr(prefix.size() + 1);

    }

    return {};

}



bool BackupOnce(const std::filesystem::path& path) {

    std::error_code ec;

    auto bak = path;

    bak += ".bak";

    if (std::filesystem::exists(bak, ec)) return true;

    if (!std::filesystem::exists(path, ec)) return true;

    std::filesystem::copy_file(path, bak, ec);

    return !ec;

}



bool ReplaceFile(const std::filesystem::path& tmp, const std::filesystem::path& target) {

    std::error_code ec;

    if (std::filesystem::exists(target, ec)) {

        std::filesystem::remove(target, ec);

    }

    ec.clear();

    std::filesystem::rename(tmp, target, ec);

    if (ec) {

        LOG_ERROR(Channels::engine, "Cannot replace '{}': {}", target.string(), ec.message());

        return false;

    }

    return true;

}



} // namespace



ItemManager::ItemManager() = default;



const char* ItemManager::RankName(int32_t rank) noexcept {

    switch (rank) {

        case 0: return "Normal";

        case 1: return "Magic";

        case 2: return "Rare";

        case 3: return "Epic";

        case 4: return "Unique";

        case 5: return "Heroic";

        case 6: return "Named";

        default: return "Unknown";

    }

}



std::string ItemManager::TypeName(int32_t rawType) {

    for (const auto& t : kTypeNames) {

        if (t.type == rawType) return t.name;

    }

    return std::format("Type {}", rawType);

}



const char* ItemManager::GroupName(ItemGroup group) noexcept {

    switch (group) {

        case ItemGroup::Weapon: return "Weapon";

        case ItemGroup::Armor: return "Armor";

        case ItemGroup::Accessory: return "Accessory";

        default: return "Other";

    }

}



std::string_view ItemManager::EquipSubTypeName(const DNItem& item) noexcept {

    if (item.equipSubType < 0) return {};

    if (item.rawType == 0) return WeaponTypeName(item.equipSubType);

    if (item.rawType == 1) return PartsTypeName(item.equipSubType);

    return {};

}



const std::vector<std::pair<int32_t, const char*>>& ItemManager::KnownItemTypes() {

    static const std::vector<std::pair<int32_t, const char*>> types = [] {

        std::vector<std::pair<int32_t, const char*>> v;

        v.reserve(std::size(kTypeNames));

        for (const auto& t : kTypeNames) v.emplace_back(t.type, t.name);

        return v;

    }();

    return types;

}



const std::vector<std::string>& ItemManager::StateTypeNames() {

    static const std::vector<std::string> names = [] {

        std::vector<std::string> v{ "(unused)" };

        v.reserve(std::size(kStateTypes) + 1);

        for (const auto& s : kStateTypes) v.emplace_back(s.name);

        return v;

    }();

    return names;

}



int32_t ItemManager::StateTypeFromIndex(int index) noexcept {

    if (index <= 0 || index > static_cast<int>(std::size(kStateTypes))) return -1;

    return kStateTypes[index - 1].type;

}



int ItemManager::StateIndexFromType(int32_t type) noexcept {

    if (type < 0) return 0;

    for (size_t i = 0; i < std::size(kStateTypes); ++i) {

        if (kStateTypes[i].type == type) return static_cast<int>(i) + 1;

    }

    return -1;

}



const char* ItemManager::StateDisplayName(int32_t stateType) noexcept {

    if (stateType < 0) return "Unused";

    for (const auto& s : kStateTypes) {

        if (s.type == stateType) return s.name;

    }

    return "Custom State";

}



ItemManager::ItemCols ItemManager::BindItemCols(const DNTTable& t) {

    ItemCols c;

    c.nameId = t.GetColumnIndex("_NameID");

    c.descId = t.GetColumnIndex("_DescriptionID");

    c.nameParam = t.GetColumnIndex("_NameIDParam");

    c.descParam = t.GetColumnIndex("_DescriptionIDParam");

    c.type = t.GetColumnIndex("_Type");

    c.rank = t.GetColumnIndex("_Rank");

    c.levelLimit = t.GetColumnIndex("_LevelLimit");

    c.heroLevelLimit = t.GetColumnIndex("_HeroLevelLimit");

    c.jobClass = t.GetColumnIndex("_NeedJobClass");

    c.overlap = t.GetColumnIndex("_OverlapCount");

    c.sell = t.GetColumnIndex("_SellAmount");

    c.icon = t.GetColumnIndex("_IconImageIndex");

    c.isCash = t.GetColumnIndex("_IsCash");

    c.ableStorage = t.GetColumnIndex("_AbleStorage");

    c.ableWStorage = t.GetColumnIndex("_AbleWStorage");

    c.gearScore = t.GetColumnIndex("_GearScore");

    c.typeParam1 = t.GetColumnIndex("_TypeParam1");

    c.typeParam2 = t.GetColumnIndex("_TypeParam2");

    c.typeParam3 = t.GetColumnIndex("_TypeParam3");
    c.dragonJewelType = t.GetColumnIndex("_dragonjeweltype");
    c.enchantId = t.GetColumnIndex("_EnchantID");
    for (size_t i = 0; i < 10; ++i) {
        c.potential[i] = t.GetColumnIndex(std::format("_Potential{}", i + 1));
    }

    for (size_t i = 0; i < kStateSlotCount; ++i) {

        c.state[i] = t.GetColumnIndex(std::format("_State{}", i + 1));

        c.prob[i] = t.GetColumnIndex(std::format("_State{}_GenProb", i + 1));

        c.minV[i] = t.GetColumnIndex(std::format("_State{}_Min", i + 1));

        c.maxV[i] = t.GetColumnIndex(std::format("_State{}_Max", i + 1));

    }

    return c;

}



ItemManager::WeaponCols ItemManager::BindWeaponCols(const DNTTable& t) {

    WeaponCols c;

    c.equipType = t.GetColumnIndex("_EquipType");

    c.durability = t.GetColumnIndex("_Durability");
    c.setItemId = t.GetColumnIndex("_SetItemID");
    c.setItemTextId = t.GetColumnIndex("_SetItemTextID");

    c.skinNames[0] = t.GetColumnIndex("_SkinName");

    c.skinNames[1] = t.GetColumnIndex("_SkinNameSub1");

    c.skinNames[2] = t.GetColumnIndex("_SkinNameSub2");

    return c;

}



ItemManager::PartsCols ItemManager::BindPartsCols(const DNTTable& t) {

    PartsCols c;

    c.parts = t.GetColumnIndex("_Parts");

    c.durability = t.GetColumnIndex("_Durability");

    c.skinId = t.GetColumnIndex("_SkinID");
    c.setItemId = t.GetColumnIndex("_SetItemID");
    c.setItemTextId = t.GetColumnIndex("_SetItemTextID");

    for (int p = 0; p < 10; ++p) {

        c.playerSkins[static_cast<size_t>(p)] = t.GetColumnIndex(std::format("_Player{}_SkinName", p + 1));

    }

    return c;

}



std::string ItemManager::ResolveText(uint32_t mid, std::string_view param) const {

    const std::string_view tmpl = m_uiStrings.GetStringView(mid);

    if (tmpl.empty()) return {};

    return ResolveTemplate(tmpl, param);

}



std::string ItemManager::ResolveTemplate(std::string_view templateText, std::string_view param) const {

    if (param.empty() || templateText.find('{') == std::string_view::npos) {

        return std::string(templateText);

    }



        std::array<std::string_view, 16> stackArgs{};

    std::vector<std::string_view> overflowArgs;

    size_t argCount = 0;



    auto pushArg = [&](std::string_view sv) {

        if (argCount < stackArgs.size()) {

            stackArgs[argCount++] = sv;

        } else {

            if (overflowArgs.empty()) {

                overflowArgs.assign(stackArgs.begin(), stackArgs.end());

            }

            overflowArgs.push_back(sv);

            ++argCount;

        }

    };



    size_t start = 0;

    while (start <= param.size()) {

        const size_t comma = param.find(',', start);

        const std::string_view raw = (comma == std::string_view::npos)

                                         ? param.substr(start)

                                         : param.substr(start, comma - start);



        const std::string_view trimmed = Trim(raw);

        bool resolvedMid = false;

        if (trimmed.size() >= 3 && trimmed.front() == '{' && trimmed.back() == '}') {

            const std::string_view inner = trimmed.substr(1, trimmed.size() - 2);

            uint32_t refMid = 0;

            const auto [p, ec] = std::from_chars(inner.data(), inner.data() + inner.size(), refMid);

            if (ec == std::errc{} && p == inner.data() + inner.size()) {

                const std::string_view refText = m_uiStrings.GetStringView(refMid);

                if (!refText.empty() || m_uiStrings.HasString(refMid)) {

                    pushArg(refText);

                    resolvedMid = true;

                }

            }

        }

        if (!resolvedMid) {

            pushArg(raw);

        }



        if (comma == std::string_view::npos) break;

        start = comma + 1;

    }



    const std::string_view* const argsPtr = overflowArgs.empty() ? stackArgs.data() : overflowArgs.data();



    std::string out;

    out.reserve(templateText.size() + 24);

    for (size_t i = 0; i < templateText.size(); ++i) {

        if (templateText[i] == '{') {

            size_t j = i + 1;

            while (j < templateText.size() && templateText[j] >= '0' && templateText[j] <= '9') ++j;

            if (j > i + 1 && j < templateText.size() && templateText[j] == '}') {

                size_t idx = 0;

                const auto [p, ec] = std::from_chars(templateText.data() + i + 1, templateText.data() + j, idx);

                if (ec == std::errc{} && idx < argCount) {

                    out.append(argsPtr[idx]);

                    i = j;

                    continue;

                }

            }

        }

        out.push_back(templateText[i]);

    }

    return out;

}



namespace {
constexpr const char* FallbackJobName(int32_t id) noexcept {
    switch (id) {
        // Base classes
        case 1:   return "Warrior";
        case 2:   return "Archer";
        case 3:   return "Sorceress";
        case 4:   return "Cleric";
        case 5:   return "Academic";
        case 6:   return "Kali";
        case 7:   return "Assassin";
        case 8:   return "Lancea";
        case 9:   return "Machina";
        case 201: return "Vandar";

        // Warrior specializations
        case 11:  return "Swordmaster";
        case 12:  return "Mercenary";
        case 23:  return "Gladiator";
        case 24:  return "Moonlord";
        case 25:  return "Barbarian";
        case 26:  return "Destroyer";
        case 75:  return "Avenger";
        case 76:  return "Dark Avenger";
        case 106: return "Knight";
        case 107: return "Mystic Knight";
        case 108: return "Grand Master";

        // Archer specializations
        case 14:  return "Bowmaster";
        case 15:  return "Acrobat";
        case 29:  return "Sniper";
        case 30:  return "Artillery";
        case 31:  return "Tempest";
        case 32:  return "Wind Walker";
        case 80:  return "Hunter";
        case 81:  return "Silver Hunter";

        // Sorceress specializations
        case 17:  return "Elemental Lord";
        case 18:  return "Force User";
        case 35:  return "Saleana";
        case 36:  return "Elestra";
        case 37:  return "Smasher";
        case 38:  return "Majesty";
        case 84:  return "Mara";
        case 85:  return "Black Mara";

        // Cleric specializations
        case 20:  return "Paladin";
        case 22:  return "Priest";
        case 41:  return "Guardian";
        case 42:  return "Crusader";
        case 43:  return "Saint";
        case 44:  return "Inquisitor";
        case 82:  return "Heretic";
        case 83:  return "Arch Heretic";

        // Academic specializations
        case 46:  return "Engineer";
        case 47:  return "Shooting Star";
        case 48:  return "Gear Master";
        case 49:  return "Alchemist";
        case 50:  return "Adept";
        case 51:  return "Physician";
        case 86:  return "Mechanic";
        case 87:  return "Ray Mechanic";

        // Kali specializations
        case 54:  return "Screamer";
        case 55:  return "Dark Summoner";
        case 56:  return "Soul Eater";
        case 57:  return "Dancer";
        case 58:  return "Blade Dancer";
        case 59:  return "Spirit Dancer";
        case 88:  return "Oracle";
        case 89:  return "Oracle Elder";

        // Assassin specializations
        case 62:  return "Chaser";
        case 63:  return "Ripper";
        case 64:  return "Raven";
        case 67:  return "Bringer";
        case 68:  return "Light Fury";
        case 69:  return "Abyss Walker";
        case 90:  return "Phantom";
        case 91:  return "Bleed Phantom";

        // Lancea specializations
        case 72:  return "Piercer";
        case 73:  return "Flurry";
        case 74:  return "Sting Breezer";
        case 92:  return "Knightess";
        case 93:  return "Avalanche";
        case 94:  return "Randgrid";
        case 98:  return "Plaga";
        case 99:  return "Vena Plaga";

        // Machina specializations
        case 77:  return "Patrona";
        case 78:  return "Defensio";
        case 79:  return "Ruina";
        case 95:  return "Launcher";
        case 96:  return "Impactor";
        case 97:  return "Luster";
        case 109: return "Beastia";
        case 110: return "Beastia Reina";

        // Vandar specializations
        case 100: return "Treasure Hunter";
        case 101: return "Wanderer";
        case 102: return "Duelist";
        case 103: return "Trickster";
        case 104: return "Revenant";
        case 105: return "Maverick";

        default:  return nullptr;
    }
}
} // namespace

std::string ItemManager::DescribeJobs(std::string_view rawJobList) const {

    if (rawJobList.empty() || rawJobList == "0") return "All Classes";



    std::string out;

    size_t start = 0;

    while (start <= rawJobList.size()) {

        const size_t semi = rawJobList.find(';', start);

        const std::string_view part = (semi == std::string_view::npos)

                                          ? rawJobList.substr(start)

                                          : rawJobList.substr(start, semi - start);

        const std::string_view trimmed = Trim(part);

        if (!trimmed.empty()) {

            int32_t id = 0;

            const auto [p, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), id);

            if (ec == std::errc{}) {

                if (id == 0) return "All Classes";



                std::string_view jobSv;

                if (m_jobNameCol >= 0) {

                    if (const auto* row = m_jobTable.GetRow(id)) {

                        const auto mid = static_cast<uint32_t>(m_jobTable.GetInt(*row, static_cast<size_t>(m_jobNameCol)));

                        jobSv = m_uiStrings.GetStringView(mid);

                    }

                }

                if (!out.empty()) out.append(", ");

                if (!jobSv.empty()) {

                    out.append(jobSv);

                } else if (const char* fb = FallbackJobName(id)) {

                    out.append(fb);

                } else {

                    out.push_back('#');

                    out.append(std::to_string(id));

                }

            }

        }

        if (semi == std::string_view::npos) break;

        start = semi + 1;

    }

    return out.empty() ? "All Classes" : out;

}



void ItemManager::ResolveItemText(DNItem& item) const {

    const std::string_view nameSv = m_uiStrings.GetStringView(item.nameId);

    const std::string_view descSv = m_uiStrings.GetStringView(item.descId);

    item.nameTemplate.assign(nameSv.data(), nameSv.size());

    item.descTemplate.assign(descSv.data(), descSv.size());



    item.name = nameSv.empty() ? std::string{} : ResolveTemplate(nameSv, item.nameParam);

    item.description = descSv.empty() ? std::string{} : ResolveTemplate(descSv, item.descParam);

    if (item.name.empty()) {

        item.name = std::format("Item {}", item.id);

    }

    item.lowerName = ToLower(item.name);

}



void ItemManager::ReadItemRowDirect(const DNTRow& row, uint16_t tableIdx, DNItem& out) const {

    const auto& table = m_itemTables[tableIdx];

    const auto& c = m_itemCols[tableIdx];

    const int32_t id = row.rowId;



    auto geti = [&](int col, int32_t def = 0) noexcept {

        return col >= 0 ? table.GetInt(row, static_cast<size_t>(col), def) : def;

    };

    auto gets = [&](int col) -> const std::string& {

        return col >= 0 ? table.GetString(row, static_cast<size_t>(col)) : kEmptyString;

    };



    out.id = id;

    out.tableIndex = tableIdx;

    out.deleted = false;

    out.nameId = static_cast<uint32_t>(geti(c.nameId));

    out.descId = static_cast<uint32_t>(geti(c.descId));

    out.nameParam = gets(c.nameParam);

    out.descParam = gets(c.descParam);

    out.rawType = geti(c.type);

    out.rank = geti(c.rank);

    out.levelLimit = geti(c.levelLimit, 1);

    out.heroLevelLimit = geti(c.heroLevelLimit);

    out.jobClass = gets(c.jobClass);

    if (out.jobClass.empty()) out.jobClass = "0";

    out.overlapCount = geti(c.overlap, 1);

    out.sellAmount = geti(c.sell);

    out.iconIndex = geti(c.icon);

    out.gearScore = geti(c.gearScore);

    out.typeParam1 = geti(c.typeParam1);

    out.typeParam2 = geti(c.typeParam2);

    out.typeParam3 = geti(c.typeParam3);

    out.isCash = geti(c.isCash) != 0;

    out.ableStorage = geti(c.ableStorage, 1) != 0;

    out.ableWStorage = geti(c.ableWStorage, 1) != 0;
    out.dragonJewelType = geti(c.dragonJewelType);
    out.enchantId = geti(c.enchantId);
    for (size_t i = 0; i < 10; ++i) {
        out.potentialIds[i] = geti(c.potential[i]);
    }



    for (size_t i = 0; i < kStateSlotCount; ++i) {

        auto& s = out.states[i];

        s.type = geti(c.state[i], -1);

        s.genProb = c.prob[i] >= 0 ? table.GetFloat(row, static_cast<size_t>(c.prob[i])) : 0.0f;

        const std::string& minStr = gets(c.minV[i]);

        const std::string& maxStr = gets(c.maxV[i]);

        s.minValue = minStr.empty() ? "0" : minStr;

        s.maxValue = maxStr.empty() ? "0" : maxStr;

    }



    // Join equipment detail row across all loaded weapontable*.dnt / partstable*.dnt.

    out.equipSubType = -1;

    out.durability = 0;

    if (out.rawType == 0) {

        const auto wIt = m_weaponTableById.find(id);

        if (wIt != m_weaponTableById.end()) {

            const auto& wt = m_weaponTables[wIt->second];

            const auto& wc = m_weaponCols[wIt->second];

            if (const auto* wRow = wt.GetRow(id)) {

                if (wc.equipType >= 0) out.equipSubType = wt.GetInt(*wRow, static_cast<size_t>(wc.equipType));

                if (wc.durability >= 0) out.durability = wt.GetInt(*wRow, static_cast<size_t>(wc.durability));
                if (wc.setItemId >= 0) out.setItemId = wt.GetInt(*wRow, static_cast<size_t>(wc.setItemId));
                if (wc.setItemTextId >= 0) out.setItemTextId = static_cast<uint32_t>(wt.GetInt(*wRow, static_cast<size_t>(wc.setItemTextId)));

            }

        }

    } else if (out.rawType == 1) {

        const auto pIt = m_partsTableById.find(id);

        if (pIt != m_partsTableById.end()) {

            const auto& pt = m_partsTables[pIt->second];

            const auto& pc = m_partsCols[pIt->second];

            if (const auto* pRow = pt.GetRow(id)) {

                if (pc.parts >= 0) out.equipSubType = pt.GetInt(*pRow, static_cast<size_t>(pc.parts));

                if (pc.durability >= 0) out.durability = pt.GetInt(*pRow, static_cast<size_t>(pc.durability));
                if (pc.setItemId >= 0) out.setItemId = pt.GetInt(*pRow, static_cast<size_t>(pc.setItemId));
                if (pc.setItemTextId >= 0) out.setItemTextId = static_cast<uint32_t>(pt.GetInt(*pRow, static_cast<size_t>(pc.setItemTextId)));

            }

        }

    }



    out.group = DeriveGroup(out.rawType, out.equipSubType);

    ResolveItemText(out);

}



void ItemManager::ReadItemRow(int32_t id, uint16_t tableIdx, DNItem& out) const {

    if (tableIdx >= m_itemTables.size()) return;

    if (const auto* row = m_itemTables[tableIdx].GetRow(id)) {

        ReadItemRowDirect(*row, tableIdx, out);

    }

}



bool ItemManager::LoadWorkspace(const std::filesystem::path& resourcePath, LoadProgressCallback onProgress) {

    const auto tStart = std::chrono::steady_clock::now();



    m_resourcePath = resourcePath;

    m_lastLoadMs = 0.0;

    m_items.clear();

    m_indexById.clear();

    m_skinCombine.clear();

    m_midRefCount.clear();

    m_modifiedItemIds.clear();

    m_itemTables.clear();

    m_itemCols.clear();

    m_dirtyItems.clear();

    m_weaponTables.clear();

    m_weaponCols.clear();

    m_dirtyWeapons.clear();

    m_weaponTableById.clear();

    m_partsTables.clear();

    m_partsCols.clear();

    m_dirtyParts.clear();

    m_partsTableById.clear();

    m_fileTable = DNTTable{};

    m_jobTable = DNTTable{};

    m_fileNameCol = -1;

    m_jobNameCol = -1;

    m_charmTables.clear();

    m_charmKeyTable = DNTTable{};

    m_charmKeys.clear();

    m_charmItemToCharmNum.clear();

    m_charmGroups.clear();

    m_allCharmNums.clear();

    m_charmRowLookup.clear();

    m_totalCharmRewards = 0;

    m_dirtyStrings = false;
    m_dragonJewelTable = DNTTable{};
    m_dragonJewelSlotTable = DNTTable{};
    m_setItemTables.clear();
    m_enchantTables.clear();
    m_potentialTables.clear();
    m_dragonJewels.clear();
    m_jewelSlotConfigs.clear();
    m_setItems.clear();
    m_allSetItemIds.clear();
    m_enchantIndex.clear();
    m_potentialIndex.clear();
    m_dirtyDragonJewelTable = false;



    {

        std::scoped_lock lock(m_modelMutex);

        m_modelIndex.clear();

        m_modelIndexBuilt.store(false, std::memory_order_release);

    }



    LOG_INFO(Channels::engine, "Scanning game resource directory: {}", resourcePath.string());



    const auto ext = resourcePath / "ext";

    const auto uiPath = resourcePath / "uistring" / "uistring.xml";

    std::error_code ec;

    if (!std::filesystem::exists(uiPath, ec) || !std::filesystem::exists(ext, ec)) {

        LOG_WARNING(Channels::engine, "Resource directory is missing uistring/uistring.xml or ext/ folder: {}", resourcePath.string());

        return false;

    }



    const auto itemFiles = DiscoverSplitTables(ext, "itemtable");

    if (itemFiles.empty()) {

        LOG_WARNING(Channels::engine, "No itemtable*.dnt files found in {}", ext.string());

        return false;

    }

    const auto weaponFiles = DiscoverSplitTables(ext, "weapontable");

    const auto partsFiles = DiscoverSplitTables(ext, "partstable");

    const auto charmItemFiles = DiscoverSplitTables(ext, "charmitemtable");

    const auto charmKeyTablePath = ext / "charmitemkeytable.dnt";

    const bool hasCharmKeyTable = std::filesystem::exists(charmKeyTablePath, ec);



    const auto fileTablePath = ext / "filetable.dnt";

    const auto jobTablePath = ext / "jobtable.dnt";

    const auto combineTablePath = ext / "partsskincombine.dnt";

    const bool hasFileTable = std::filesystem::exists(fileTablePath, ec);

    const bool hasJobTable = std::filesystem::exists(jobTablePath, ec);

    const bool hasCombineTable = std::filesystem::exists(combineTablePath, ec);

    const auto dragonJewelPath = ext / "dragonjeweltable.dnt";
    const auto dragonJewelSlotPath = ext / "dragonjewelslottable.dnt";
    const bool hasDragonJewelTable = std::filesystem::exists(dragonJewelPath, ec);
    const bool hasDragonJewelSlotTable = std::filesystem::exists(dragonJewelSlotPath, ec);

    const auto setItemFiles = DiscoverSplitTables(ext, "setitemtable");
    const auto enchantFiles = DiscoverSplitTables(ext, "enchanttable");
    const auto potentialFiles = DiscoverSplitTables(ext, "potentialtable");

    std::vector<DNTTable> loadedSetItems(setItemFiles.size());
    std::vector<DNTTable> loadedEnchants(enchantFiles.size());
    std::vector<DNTTable> loadedPotentials(potentialFiles.size());



            std::vector<DNTTable> loadedItems(itemFiles.size());

    std::vector<uint8_t> okItems(itemFiles.size(), 0);

    std::vector<DNTTable> loadedWeapons(weaponFiles.size());

    std::vector<uint8_t> okWeapons(weaponFiles.size(), 0);

    std::vector<DNTTable> loadedParts(partsFiles.size());

    std::vector<uint8_t> okParts(partsFiles.size(), 0);

    DNTTable combineTable;

    bool okCombine = false;



    const size_t charmPairCount = charmItemFiles.size();

    std::vector<std::filesystem::path> charmProbFiles(charmPairCount);

    for (size_t i = 0; i < charmPairCount; ++i) {

        const std::string sfx = ExtractTableSuffix(charmItemFiles[i].filename().string(), "charmitemtable");

        charmProbFiles[i] = ext / (sfx.empty() ? "charmitemprobtable.dnt" : std::format("charmitemprobtable_{}.dnt", sfx));

    }

    std::vector<DNTTable> loadedCharmItems(charmPairCount);

    std::vector<DNTTable> loadedCharmProbs(charmPairCount);



    const size_t baseItemTask = 4;
    const size_t basePartsTask = baseItemTask + itemFiles.size();
    const size_t baseWeaponTask = basePartsTask + partsFiles.size();
    const size_t baseCharmTask = baseWeaponTask + weaponFiles.size();
    const size_t baseCharmKeyTask = baseCharmTask + charmPairCount * 2;
    const size_t baseDjTask = baseCharmKeyTask + (hasCharmKeyTable ? 1 : 0);
    const size_t baseDjSlotTask = baseDjTask + (hasDragonJewelTable ? 1 : 0);
    const size_t baseSetItemTask = baseDjSlotTask + (hasDragonJewelSlotTable ? 1 : 0);
    const size_t baseEnchantTask = baseSetItemTask + setItemFiles.size();
    const size_t basePotentialTask = baseEnchantTask + enchantFiles.size();
    const size_t totalFileTasks = basePotentialTask + potentialFiles.size();



    if (onProgress) {

        onProgress(0.12f, "Loading Game Resource Tables...",

                   std::format("Loading {} item, weapon, parts, and string tables", totalFileTasks));

    }



    std::atomic<size_t> doneTasks{0};

    ParallelFor(totalFileTasks, [&](size_t taskIdx) {

        if (taskIdx == 0) {

            m_uiStrings.LoadFromFile(uiPath);

        } else if (taskIdx == 1) {

            if (hasFileTable && m_fileTable.LoadFromFile(fileTablePath)) {

                m_fileNameCol = m_fileTable.GetColumnIndex("_FileName");

            }

        } else if (taskIdx == 2) {

            if (hasJobTable && m_jobTable.LoadFromFile(jobTablePath)) {

                m_jobNameCol = m_jobTable.GetColumnIndex("_JobName");

            }

        } else if (taskIdx == 3) {

            if (hasCombineTable) {

                okCombine = combineTable.LoadFromFile(combineTablePath);

            }

        } else if (taskIdx < basePartsTask) {

            const size_t i = taskIdx - baseItemTask;

            okItems[i] = loadedItems[i].LoadFromFile(itemFiles[i]) ? 1 : 0;

        } else if (taskIdx < baseWeaponTask) {

            const size_t i = taskIdx - basePartsTask;

            okParts[i] = loadedParts[i].LoadFromFile(partsFiles[i]) ? 1 : 0;

        } else if (taskIdx < baseCharmTask) {

            const size_t i = taskIdx - baseWeaponTask;

            okWeapons[i] = loadedWeapons[i].LoadFromFile(weaponFiles[i]) ? 1 : 0;

        } else if (taskIdx < baseCharmTask + charmPairCount) {

            const size_t i = taskIdx - baseCharmTask;

            loadedCharmItems[i].LoadFromFile(charmItemFiles[i]);

        } else if (taskIdx < baseCharmTask + charmPairCount * 2) {

            const size_t i = taskIdx - (baseCharmTask + charmPairCount);

            if (std::filesystem::exists(charmProbFiles[i])) {

                loadedCharmProbs[i].LoadFromFile(charmProbFiles[i]);

            }

        } else if (taskIdx == baseCharmKeyTask) {
            if (hasCharmKeyTable) {
                m_charmKeyTable.LoadFromFile(charmKeyTablePath);
            }
        } else if (taskIdx == baseDjTask) {
            if (hasDragonJewelTable) {
                m_dragonJewelTable.LoadFromFile(dragonJewelPath);
            }
        } else if (taskIdx == baseDjSlotTask) {
            if (hasDragonJewelSlotTable) {
                m_dragonJewelSlotTable.LoadFromFile(dragonJewelSlotPath);
            }
        } else if (taskIdx >= baseSetItemTask && taskIdx < baseEnchantTask) {
            const size_t i = taskIdx - baseSetItemTask;
            loadedSetItems[i].LoadFromFile(setItemFiles[i]);
        } else if (taskIdx >= baseEnchantTask && taskIdx < basePotentialTask) {
            const size_t i = taskIdx - baseEnchantTask;
            loadedEnchants[i].LoadFromFile(enchantFiles[i]);
        } else if (taskIdx >= basePotentialTask && taskIdx < totalFileTasks) {
            const size_t i = taskIdx - basePotentialTask;
            loadedPotentials[i].LoadFromFile(potentialFiles[i]);
        }



        const size_t done = doneTasks.fetch_add(1) + 1;

        if (onProgress && (done % 2 == 0 || done == totalFileTasks)) {

            const float p = 0.15f + 0.65f * (static_cast<float>(done) / static_cast<float>(totalFileTasks));

            onProgress(p, "Loading Game Resource Tables...",

                       std::format("Processed {}/{} DNT table slices", done, totalFileTasks));

        }

    });




    // --- Index Dragon Jewel Sockets (_DragonJewelID -> DragonJewelSlotConfig) ---
    if (m_dragonJewelSlotTable.HasSchema()) {
        const int cSlotId = m_dragonJewelSlotTable.GetColumnIndex("_DragonJewelID");
        const int cSlotRatio = m_dragonJewelSlotTable.GetColumnIndex("_DragonJewelRatio");
        const int cSlot1 = m_dragonJewelSlotTable.GetColumnIndex("_DragonJewelSlot1");
        const int cSlot2 = m_dragonJewelSlotTable.GetColumnIndex("_DragonJewelSlot2");
        const int cSlot3 = m_dragonJewelSlotTable.GetColumnIndex("_DragonJewelSlot3");
        const int cSlot4 = m_dragonJewelSlotTable.GetColumnIndex("_DragonJewelSlot4");
        const int cSlotEnchant = m_dragonJewelSlotTable.GetColumnIndex("_IsEnchanted");

        if (cSlotId >= 0) {
            m_jewelSlotConfigs.reserve(m_dragonJewelSlotTable.GetRowCount() + 8);
            for (const auto& r : m_dragonJewelSlotTable.GetRows()) {
                const int32_t jid = m_dragonJewelSlotTable.GetInt(r, static_cast<size_t>(cSlotId));
                if (jid <= 0) continue;
                DragonJewelSlotConfig cfg;
                cfg.jewelTypeId = jid;
                cfg.ratio = cSlotRatio >= 0 ? m_dragonJewelSlotTable.GetFloat(r, static_cast<size_t>(cSlotRatio)) : 1.0f;
                cfg.slots[0] = cSlot1 >= 0 ? m_dragonJewelSlotTable.GetInt(r, static_cast<size_t>(cSlot1)) : 0;
                cfg.slots[1] = cSlot2 >= 0 ? m_dragonJewelSlotTable.GetInt(r, static_cast<size_t>(cSlot2)) : 0;
                cfg.slots[2] = cSlot3 >= 0 ? m_dragonJewelSlotTable.GetInt(r, static_cast<size_t>(cSlot3)) : 0;
                cfg.slots[3] = cSlot4 >= 0 ? m_dragonJewelSlotTable.GetInt(r, static_cast<size_t>(cSlot4)) : 0;
                cfg.isEnchanted = cSlotEnchant >= 0 ? (m_dragonJewelSlotTable.GetInt(r, static_cast<size_t>(cSlotEnchant)) != 0) : false;
                m_jewelSlotConfigs[jid] = cfg;
            }
        }
    }

    // --- Index Dragon Jewels (Item ID -> DragonJewelInfo) ---
    if (m_dragonJewelTable.HasSchema()) {
        const int cDjType = m_dragonJewelTable.GetColumnIndex("_Type");
        const int cDjPeriod = m_dragonJewelTable.GetColumnIndex("_Period");
        const int cDjApp1 = m_dragonJewelTable.GetColumnIndex("_ApplicableValue");
        const int cDjApp2 = m_dragonJewelTable.GetColumnIndex("_ApplicableValue2");
        const int cDjSeal = m_dragonJewelTable.GetColumnIndex("_CanApplySealedItem");
        const int cDjCharge = m_dragonJewelTable.GetColumnIndex("_Charge");
        const int cDjElem = m_dragonJewelTable.GetColumnIndex("_ElementalType");

        if (cDjType >= 0) {
            m_dragonJewels.reserve(m_dragonJewelTable.GetRowCount() + 16);
            for (const auto& r : m_dragonJewelTable.GetRows()) {
                DragonJewelInfo info;
                info.itemId = r.rowId;
                info.type = cDjType >= 0 ? m_dragonJewelTable.GetInt(r, static_cast<size_t>(cDjType)) : 0;
                info.period = cDjPeriod >= 0 ? m_dragonJewelTable.GetInt(r, static_cast<size_t>(cDjPeriod)) : 0;
                info.applicableSlots = cDjApp1 >= 0 ? m_dragonJewelTable.GetInt(r, static_cast<size_t>(cDjApp1)) : 0;
                info.applicableSlots2 = cDjApp2 >= 0 ? m_dragonJewelTable.GetInt(r, static_cast<size_t>(cDjApp2)) : 0;
                info.canApplySealedItem = cDjSeal >= 0 ? (m_dragonJewelTable.GetInt(r, static_cast<size_t>(cDjSeal)) != 0) : false;
                info.charge = cDjCharge >= 0 ? m_dragonJewelTable.GetInt(r, static_cast<size_t>(cDjCharge)) : 0;
                info.elementalType = cDjElem >= 0 ? m_dragonJewelTable.GetInt(r, static_cast<size_t>(cDjElem)) : -1;
                m_dragonJewels[info.itemId] = info;
            }
        }
    }

    // --- Index Set Items (setitemtable.dnt / setitemtable_cash.dnt) ---
    m_setItemTables = std::move(loadedSetItems);
    m_setItems.clear();
    m_allSetItemIds.clear();

    for (size_t tIdx = 0; tIdx < m_setItemTables.size(); ++tIdx) {
        const auto& st = m_setItemTables[tIdx];
        if (!st.HasSchema()) continue;

        for (const auto& r : st.GetRows()) {
            const int32_t setId = r.rowId;
            if (setId <= 0) continue;

            auto& grp = m_setItems[setId];
            grp.setItemId = setId;

            for (int i = 1; i <= 12; ++i) {
                const int cNeed = st.GetColumnIndex(std::format("_NeedSetNum{}", i));
                const int cState = st.GetColumnIndex(std::format("_State{}", i));
                const int cVal = st.GetColumnIndex(std::format("_State{}Value", i));
                if (cNeed >= 0 && cState >= 0 && cVal >= 0) {
                    const int32_t needNum = st.GetInt(r, static_cast<size_t>(cNeed));
                    const int32_t stateType = st.GetInt(r, static_cast<size_t>(cState));
                    const std::string& val = st.GetString(r, static_cast<size_t>(cVal));
                    if (needNum > 0 && stateType >= 0 && !val.empty() && val != "0") {
                        grp.bonuses.push_back({ needNum, stateType, val });
                    }
                }
            }

            for (int i = 1; i <= 9; ++i) {
                const int cNeed = st.GetColumnIndex(std::format("_NeededSkillSetNum{}", i));
                const int cSkill = st.GetColumnIndex(std::format("_SkillID{}", i));
                const int cLvl = st.GetColumnIndex(std::format("_SkillLevel{}", i));
                if (cNeed >= 0 && cSkill >= 0 && cLvl >= 0) {
                    const int32_t needNum = st.GetInt(r, static_cast<size_t>(cNeed));
                    const int32_t skillId = st.GetInt(r, static_cast<size_t>(cSkill));
                    const int32_t skillLevel = st.GetInt(r, static_cast<size_t>(cLvl));
                    if (needNum > 0 && skillId > 0) {
                        grp.skillBonuses.push_back({ needNum, skillId, skillLevel });
                    }
                }
            }

            const int cTransItem = st.GetColumnIndex("_TransformItemID");
            const int cTransParts = st.GetColumnIndex("_TransformPartsNum");
            if (cTransItem >= 0) grp.transformItemId = st.GetInt(r, static_cast<size_t>(cTransItem));
            if (cTransParts >= 0) grp.transformPartsNum = st.GetInt(r, static_cast<size_t>(cTransParts));
        }
    }

    // --- Index Enchant Tables (_EnchantID -> rows) ---
    m_enchantTables = std::move(loadedEnchants);
    m_enchantIndex.clear();
    for (uint16_t tIdx = 0; tIdx < static_cast<uint16_t>(m_enchantTables.size()); ++tIdx) {
        const auto& et = m_enchantTables[tIdx];
        if (!et.HasSchema()) continue;
        const int cEid = et.GetColumnIndex("_EnchantID");
        if (cEid < 0) continue;

        for (uint32_t rIdx = 0; rIdx < static_cast<uint32_t>(et.GetRowCount()); ++rIdx) {
            const auto& r = et.GetRows()[rIdx];
            const int32_t eid = et.GetInt(r, static_cast<size_t>(cEid));
            if (eid > 0) {
                m_enchantIndex[eid].push_back({ tIdx, rIdx });
            }
        }
    }

    // --- Index Potential Tables (_PotentialID -> rows) ---
    m_potentialTables = std::move(loadedPotentials);
    m_potentialIndex.clear();
    for (uint16_t tIdx = 0; tIdx < static_cast<uint16_t>(m_potentialTables.size()); ++tIdx) {
        const auto& pt = m_potentialTables[tIdx];
        if (!pt.HasSchema()) continue;
        const int cPid = pt.GetColumnIndex("_PotentialID");
        if (cPid < 0) continue;

        for (uint32_t rIdx = 0; rIdx < static_cast<uint32_t>(pt.GetRowCount()); ++rIdx) {
            const auto& r = pt.GetRows()[rIdx];
            const int32_t pid = pt.GetInt(r, static_cast<size_t>(cPid));
            if (pid > 0) {
                m_potentialIndex[pid].push_back({ tIdx, rIdx });
            }
        }
    }

        if (okCombine) {

        const int cSkinId = combineTable.GetColumnIndex("_SkinID");

        const int cFileId = combineTable.GetColumnIndex("_Player_SkinName");

        if (cSkinId >= 0 && cFileId >= 0) {

            m_skinCombine.reserve(combineTable.GetRowCount());

            for (const auto& r : combineTable.GetRows()) {

                const int32_t sid = combineTable.GetInt(r, static_cast<size_t>(cSkinId));

                const int32_t fid = combineTable.GetInt(r, static_cast<size_t>(cFileId));

                if (sid > 0 && fid > 0) m_skinCombine.try_emplace(sid, fid);

            }

        }

    }



        size_t totalWeaponRows = 0;

    for (size_t i = 0; i < loadedWeapons.size(); ++i) {

        if (okWeapons[i]) totalWeaponRows += loadedWeapons[i].GetRowCount();

    }

    m_weaponTables.reserve(loadedWeapons.size());

    m_weaponCols.reserve(loadedWeapons.size());

    m_weaponTableById.reserve(totalWeaponRows + 32);

    for (size_t i = 0; i < loadedWeapons.size(); ++i) {

        if (!okWeapons[i]) continue;

        const auto idx = static_cast<uint16_t>(m_weaponTables.size());

        for (const auto& r : loadedWeapons[i].GetRows()) {

            m_weaponTableById.try_emplace(r.rowId, idx);

        }

        m_weaponCols.push_back(BindWeaponCols(loadedWeapons[i]));

        m_weaponTables.push_back(std::move(loadedWeapons[i]));

    }

    m_dirtyWeapons.assign(m_weaponTables.size(), false);



    size_t totalPartsRows = 0;

    for (size_t i = 0; i < loadedParts.size(); ++i) {

        if (okParts[i]) totalPartsRows += loadedParts[i].GetRowCount();

    }

    m_partsTables.reserve(loadedParts.size());

    m_partsCols.reserve(loadedParts.size());

    m_partsTableById.reserve(totalPartsRows + 32);

    for (size_t i = 0; i < loadedParts.size(); ++i) {

        if (!okParts[i]) continue;

        const auto idx = static_cast<uint16_t>(m_partsTables.size());

        for (const auto& r : loadedParts[i].GetRows()) {

            m_partsTableById.try_emplace(r.rowId, idx);

        }

        m_partsCols.push_back(BindPartsCols(loadedParts[i]));

        m_partsTables.push_back(std::move(loadedParts[i]));

    }

    m_dirtyParts.assign(m_partsTables.size(), false);



    size_t totalExpectedItems = 0;

    m_itemTables.reserve(loadedItems.size());

    m_itemCols.reserve(loadedItems.size());

    for (size_t i = 0; i < loadedItems.size(); ++i) {

        if (!okItems[i]) continue;

        totalExpectedItems += loadedItems[i].GetRowCount();

        m_itemCols.push_back(BindItemCols(loadedItems[i]));

        m_itemTables.push_back(std::move(loadedItems[i]));

    }

    m_dirtyItems.assign(m_itemTables.size(), false);



    struct RowCoord {

        uint16_t tableIdx;

        uint32_t rowIdx;

    };

    std::vector<RowCoord> coords;

    coords.reserve(totalExpectedItems);

    m_indexById.reserve(totalExpectedItems + 64);



    for (size_t tIdx = 0; tIdx < m_itemTables.size(); ++tIdx) {

        const auto& rows = m_itemTables[tIdx].GetRows();

        for (uint32_t rIdx = 0; rIdx < static_cast<uint32_t>(rows.size()); ++rIdx) {

            const int32_t rowId = rows[rIdx].rowId;

            if (m_indexById.try_emplace(rowId, coords.size()).second) {

                coords.push_back(RowCoord{ static_cast<uint16_t>(tIdx), rIdx });

            }

        }

    }



        if (onProgress) {

        onProgress(0.85f, "Indexing Item Records...",

                   std::format("Extracting {} items across all partitions", coords.size()));

    }

    m_items.resize(coords.size());

    ParallelFor(coords.size(), [&](size_t idx) {

        const RowCoord rc = coords[idx];

        ReadItemRowDirect(m_itemTables[rc.tableIdx].GetRows()[rc.rowIdx], rc.tableIdx, m_items[idx]);

    }, /*chunkSize=*/512);



        m_midRefCount.reserve(m_items.size() + 1024);
    for (const auto& item : m_items) {
        if (item.nameId > 0) ++m_midRefCount[item.nameId];
        if (item.descId > 0 && item.descId != item.nameId) ++m_midRefCount[item.descId];

        if (item.setItemId > 0) {
            auto sit = m_setItems.find(item.setItemId);
            if (sit != m_setItems.end()) {
                sit->second.memberItemIds.push_back(item.id);
                if (item.setItemTextId > 0 && sit->second.setName.empty()) {
                    sit->second.setNameMid = item.setItemTextId;
                    sit->second.setName = m_uiStrings.GetString(item.setItemTextId);
                }
            }
        }
    }

    m_allSetItemIds.reserve(m_setItems.size());
    for (const auto& [sid, grp] : m_setItems) {
        m_allSetItemIds.push_back(sid);
    }
    std::sort(m_allSetItemIds.begin(), m_allSetItemIds.end());



    if (onProgress) {

        onProgress(0.82f, "Indexing Charm & Drop Tables...", "Resolving drop pools and reward sets");

    }



    // --- Index Charm & Drop Probability Tables ---

    m_charmTables.reserve(charmPairCount);

    for (size_t p = 0; p < charmPairCount; ++p) {

        if (!loadedCharmItems[p].HasSchema()) continue;

        CharmTablePair pair;

        pair.itemPath = charmItemFiles[p];

        pair.probPath = charmProbFiles[p];

        pair.itemTable = std::move(loadedCharmItems[p]);

        pair.probTable = std::move(loadedCharmProbs[p]);



        pair.cItemId = pair.itemTable.GetColumnIndex("_ItemID");

        pair.cCount = pair.itemTable.GetColumnIndex("_Count");

        pair.cCharmNum = pair.itemTable.GetColumnIndex("_CharmNum");

        pair.cMsg = pair.itemTable.GetColumnIndex("_Msg");

        pair.cGold = pair.itemTable.GetColumnIndex("_Gold");

        pair.cPeriod = pair.itemTable.GetColumnIndex("_Period");

        pair.cMsgType = pair.itemTable.GetColumnIndex("_MsgType");

        pair.cItemNameId = pair.itemTable.GetColumnIndex("_ItemNameID");

        pair.cProbInItem = pair.itemTable.GetColumnIndex("_Prob");

        pair.cProbInProb = pair.probTable.HasSchema() ? pair.probTable.GetColumnIndex("_Prob") : -1;

        pair.cPotenProb = pair.probTable.HasSchema() ? pair.probTable.GetColumnIndex("_PotenProb") : pair.itemTable.GetColumnIndex("_PotenProb");



        const size_t pairIdx = m_charmTables.size();

        const std::string tblName = pair.itemTable.GetFileName();



        for (const auto& r : pair.itemTable.GetRows()) {

            CharmReward rew;

            rew.rowId = r.rowId;

            rew.sourceTable = tblName;

            rew.tableIndex = pairIdx;

            rew.itemId = pair.cItemId >= 0 ? pair.itemTable.GetInt(r, static_cast<size_t>(pair.cItemId)) : 0;

            rew.count = pair.cCount >= 0 ? std::max<int32_t>(1, pair.itemTable.GetInt(r, static_cast<size_t>(pair.cCount))) : 1;

            rew.charmNum = pair.cCharmNum >= 0 ? pair.itemTable.GetInt(r, static_cast<size_t>(pair.cCharmNum)) : 0;

            rew.broadcast = pair.cMsg >= 0 ? (pair.itemTable.GetInt(r, static_cast<size_t>(pair.cMsg)) != 0) : false;

            rew.gold = pair.cGold >= 0 ? pair.itemTable.GetInt(r, static_cast<size_t>(pair.cGold)) : 0;

            rew.period = pair.cPeriod >= 0 ? pair.itemTable.GetInt(r, static_cast<size_t>(pair.cPeriod)) : 0;

            rew.msgType = pair.cMsgType >= 0 ? pair.itemTable.GetInt(r, static_cast<size_t>(pair.cMsgType)) : 0;



            int32_t weight = 0;

            int32_t potenProb = 0;

            if (pair.probTable.HasSchema()) {

                if (const auto* prRow = pair.probTable.GetRow(r.rowId)) {

                    if (pair.cProbInProb >= 0) weight = pair.probTable.GetInt(*prRow, static_cast<size_t>(pair.cProbInProb));

                    if (pair.cPotenProb >= 0) potenProb = pair.probTable.GetInt(*prRow, static_cast<size_t>(pair.cPotenProb));

                }

            }

            if (weight == 0 && pair.cProbInItem >= 0) {

                weight = pair.itemTable.GetInt(r, static_cast<size_t>(pair.cProbInItem));

            }

            rew.probWeight = weight;

            rew.potenProb = potenProb;



            // Resolve item metadata

            if (const auto* dItem = GetItem(rew.itemId)) {

                rew.itemName = dItem->name;

                rew.itemRank = dItem->rank;

                rew.itemIconIndex = dItem->iconIndex;

            } else {

                const uint32_t nid = pair.cItemNameId >= 0 ? static_cast<uint32_t>(pair.itemTable.GetInt(r, static_cast<size_t>(pair.cItemNameId))) : 0;

                if (nid > 0) rew.itemName = m_uiStrings.GetString(nid);

                if (rew.itemName.empty()) rew.itemName = std::format("Item #{}", rew.itemId);

            }



            auto& group = m_charmGroups[rew.charmNum];

            group.charmNum = rew.charmNum;

            if (group.sourceTable.empty()) group.sourceTable = tblName;

            group.rewards.push_back(rew);



            m_charmRowLookup[rew.rowId] = { pairIdx, rew.charmNum };

            ++m_totalCharmRewards;

        }



        m_charmTables.push_back(std::move(pair));

    }



    // Compute normalized drop percentages and cumulative thresholds for all groups

    m_allCharmNums.reserve(m_charmGroups.size());

    for (auto& [cNum, group] : m_charmGroups) {

        m_allCharmNums.push_back(cNum);

        int64_t totalW = 0;

        for (const auto& rew : group.rewards) {

            if (rew.probWeight > 0) totalW += rew.probWeight;

        }

        group.totalWeight = totalW;

        double cum = 0.0;

        for (auto& rew : group.rewards) {

            if (totalW > 0 && rew.probWeight > 0) {

                rew.dropRatePercent = (static_cast<double>(rew.probWeight) / static_cast<double>(totalW)) * 100.0;

                cum += rew.dropRatePercent;

                rew.cumulativePercent = std::min(100.0, cum);

            } else {

                rew.dropRatePercent = 0.0;

                rew.cumulativePercent = cum;

            }

        }

    }

    std::sort(m_allCharmNums.begin(), m_allCharmNums.end());



    // Parse charmitemkeytable.dnt

    if (m_charmKeyTable.HasSchema()) {

        const int cCharmId = m_charmKeyTable.GetColumnIndex("_CharmItemID");

        std::array<int, 5> cKeys{ -1, -1, -1, -1, -1 };

        std::array<int, 5> cCosts{ -1, -1, -1, -1, -1 };

        for (int k = 0; k < 5; ++k) {

            cKeys[static_cast<size_t>(k)] = m_charmKeyTable.GetColumnIndex(std::format("_Key{}", k + 1));

            cCosts[static_cast<size_t>(k)] = m_charmKeyTable.GetColumnIndex(std::format("_Key{}cost", k + 1));

        }



        m_charmKeys.reserve(m_charmKeyTable.GetRowCount());

        for (const auto& r : m_charmKeyTable.GetRows()) {

            const int32_t cid = cCharmId >= 0 ? m_charmKeyTable.GetInt(r, static_cast<size_t>(cCharmId)) : 0;

            if (cid <= 0) continue;

            CharmKeyInfo kInfo;

            kInfo.charmItemId = cid;

            for (int k = 0; k < 5; ++k) {

                if (cKeys[static_cast<size_t>(k)] >= 0) kInfo.keys[static_cast<size_t>(k)] = m_charmKeyTable.GetInt(r, static_cast<size_t>(cKeys[static_cast<size_t>(k)]));

                if (cCosts[static_cast<size_t>(k)] >= 0) kInfo.keyCosts[static_cast<size_t>(k)] = m_charmKeyTable.GetInt(r, static_cast<size_t>(cCosts[static_cast<size_t>(k)]));

            }

            m_charmKeys[cid] = kInfo;

        }

    }



    // Map item IDs to charm numbers

    for (const auto& item : m_items) {

        if (item.typeParam1 > 0 && m_charmGroups.contains(item.typeParam1)) {

            m_charmItemToCharmNum[item.id] = item.typeParam1;

        }

    }



    if (onProgress) {

        onProgress(0.95f, "Item Index Complete",

                   std::format("Successfully indexed {} items across {} tables", m_items.size(), m_itemTables.size()));

    }

    const auto tEnd = std::chrono::steady_clock::now();

    m_lastLoadMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();



    LOG_INFO(Channels::engine,

             "ItemManager loaded {} items across {} item tables (weapons: {}, parts: {}, charms: {} groups / {} rewards, strings: {}) in {:.1f} ms.",

             m_items.size(), m_itemTables.size(), m_weaponTables.size(), m_partsTables.size(),

             m_charmGroups.size(), m_totalCharmRewards, m_uiStrings.GetCount(), m_lastLoadMs);

    return !m_items.empty();

}



const std::string& ItemManager::GetItemTableName(size_t index) const noexcept {

    return index < m_itemTables.size() ? m_itemTables[index].GetFileName() : kEmptyString;

}



int ItemManager::FindItemTableByName(std::string_view fileName) const {

    for (size_t i = 0; i < m_itemTables.size(); ++i) {

        if (EqualsIgnoreCaseAscii(m_itemTables[i].GetFileName(), fileName)) {

            return static_cast<int>(i);

        }

    }

    return -1;

}



const DNTTable* ItemManager::GetDetailTableForItem(const DNItem& item) const {

    if (item.rawType == 0) {

        const auto it = m_weaponTableById.find(item.id);

        return it != m_weaponTableById.end() ? &m_weaponTables[it->second] : nullptr;

    }

    if (item.rawType == 1) {

        const auto it = m_partsTableById.find(item.id);

        return it != m_partsTableById.end() ? &m_partsTables[it->second] : nullptr;

    }

    return nullptr;

}



DNTTable* ItemManager::FindMutableDetailTableForItem(const DNItem& item, size_t* outIdx) {

    if (item.rawType == 0) {

        const auto it = m_weaponTableById.find(item.id);

        if (it != m_weaponTableById.end()) {

            if (outIdx) *outIdx = it->second;

            return &m_weaponTables[it->second];

        }

    } else if (item.rawType == 1) {

        const auto it = m_partsTableById.find(item.id);

        if (it != m_partsTableById.end()) {

            if (outIdx) *outIdx = it->second;

            return &m_partsTables[it->second];

        }

    }

    return nullptr;

}



int ItemManager::FindMatchingDetailTableIndex(size_t itemTableIdx, bool weapon) const {

    const auto& detailTables = weapon ? m_weaponTables : m_partsTables;

    const std::string_view detailPrefix = weapon ? "weapontable" : "partstable";

    if (detailTables.empty()) return -1;



    if (itemTableIdx < m_itemTables.size()) {

        const std::string suffix = ExtractTableSuffix(m_itemTables[itemTableIdx].GetFileName(), "itemtable");

        const std::string expected = suffix.empty()

                                         ? std::format("{}.dnt", detailPrefix)

                                         : std::format("{}_{}.dnt", detailPrefix, suffix);

        for (size_t i = 0; i < detailTables.size(); ++i) {

            if (EqualsIgnoreCaseAscii(detailTables[i].GetFileName(), expected) && detailTables[i].HasSchema()) {

                return static_cast<int>(i);

            }

        }

    }



    // Fallback to `_equipment.dnt` or primary `.dnt` with valid schema (handles 0-byte placeholders like weapontable_reboot.dnt).

    for (size_t i = 0; i < detailTables.size(); ++i) {

        if (detailTables[i].HasSchema()) return static_cast<int>(i);

    }

    return -1;

}



const DNItem* ItemManager::GetItem(int32_t id) const {

    const auto it = m_indexById.find(id);

    if (it == m_indexById.end()) return nullptr;

    const DNItem& item = m_items[it->second];

    return item.deleted ? nullptr : &item;

}



ItemQueryResult ItemManager::Search(const ItemSearchFilter& filter) const {

    ItemQueryResult result;

    std::string_view rawKw = Trim(filter.keyword);

    if (!rawKw.empty() && rawKw.front() == '#') rawKw.remove_prefix(1);

    std::string kw = ToLower(rawKw);



    // Smart query prefixes: mid:<num>, lv>=<num>, lv<=<num>, lv:<num>, job:<num>

    uint32_t filterMid = 0;

    bool hasMidQuery = false;

    int32_t minLvQuery = -1, maxLvQuery = -1;

    std::string jobQuery;



    auto parseLeadingNum = [](std::string_view s, int32_t& out) noexcept -> bool {

        s = Trim(s);

        if (s.empty()) return false;

        const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), out);

        return ec == std::errc{} && p == s.data() + s.size();

    };



    if (kw.starts_with("mid:")) {

        int32_t m = 0;

        if (parseLeadingNum(std::string_view(kw).substr(4), m) && m > 0) {

            filterMid = static_cast<uint32_t>(m);

            hasMidQuery = true;

            kw.clear();

        }

    } else if (kw.starts_with("lv>=")) {

        if (parseLeadingNum(std::string_view(kw).substr(4), minLvQuery)) kw.clear();

    } else if (kw.starts_with("lv<=")) {

        if (parseLeadingNum(std::string_view(kw).substr(4), maxLvQuery)) kw.clear();

    } else if (kw.starts_with("lv:")) {

        int32_t exactLv = -1;

        if (parseLeadingNum(std::string_view(kw).substr(3), exactLv)) {

            minLvQuery = maxLvQuery = exactLv;

            kw.clear();

        }

    } else if (kw.starts_with("job:")) {

        jobQuery = std::string(Trim(std::string_view(kw).substr(4)));

        if (!jobQuery.empty()) kw.clear();

    }



    int32_t exactIdQuery = -1;

    bool isNumericQuery = !kw.empty();

    for (char ch : kw) {

        if (ch < '0' || ch > '9') {

            isNumericQuery = false;

            break;

        }

    }

    if (isNumericQuery) {

        std::from_chars(kw.data(), kw.data() + kw.size(), exactIdQuery);

    }



    const bool needsCustomSort = (filter.sortBy != ItemSortColumn::Id) || !filter.sortAscending;

    if (filter.limit == 0 || needsCustomSort) {

        result.items.reserve(filter.modifiedOnly

                                 ? m_modifiedItemIds.size()

                                 : (filter.tableFilter >= 0 && static_cast<size_t>(filter.tableFilter) < m_itemTables.size()

                                        ? m_itemTables[static_cast<size_t>(filter.tableFilter)].GetRowCount()

                                        : m_items.size() / (filter.groupFilter >= 0 ? 3 : 1)));

    } else {

        result.items.reserve(std::min<size_t>(filter.limit, 4096));

    }



    char idBuf[16];

    const bool hasKw = !kw.empty();

    for (const auto& item : m_items) {

        if (item.deleted) continue;

        if (filter.modifiedOnly && !m_modifiedItemIds.contains(item.id)) continue;

        if (filter.tableFilter >= 0 && static_cast<int>(item.tableIndex) != filter.tableFilter) continue;

        if (filter.groupFilter >= 0 && static_cast<int>(item.group) != filter.groupFilter) continue;

        if (filter.rankFilter >= 0 && item.rank != filter.rankFilter) continue;

        if (hasMidQuery && item.nameId != filterMid && item.descId != filterMid) continue;

        if (minLvQuery >= 0 && item.levelLimit < minLvQuery) continue;

        if (maxLvQuery >= 0 && item.levelLimit > maxLvQuery) continue;

        if (!jobQuery.empty()) {

            bool jobHit = false;

            std::string_view rem = item.jobClass;

            while (!rem.empty()) {

                const size_t semi = rem.find(';');

                const std::string_view tok = Trim(semi == std::string_view::npos ? rem : rem.substr(0, semi));

                if (tok == jobQuery) {

                    jobHit = true;

                    break;

                }

                if (semi == std::string_view::npos) break;

                rem.remove_prefix(semi + 1);

            }

            if (!jobHit) continue;

        }



        if (hasKw) {

            bool matched = false;

            if (isNumericQuery && item.id == exactIdQuery) {

                matched = true;

            } else if (ContainsSubstringFast(item.lowerName, kw)) {

                matched = true;

            } else if (isNumericQuery) {

                const auto [p, ec] = std::to_chars(idBuf, idBuf + sizeof(idBuf), item.id);

                if (ec == std::errc{}) {

                    matched = ContainsSubstringFast(std::string_view(idBuf, static_cast<size_t>(p - idBuf)), kw);

                }

            }

            if (!matched) continue;

        }



        ++result.totalMatches;

        if (needsCustomSort || filter.limit == 0 || result.items.size() < filter.limit) {

            result.items.push_back(&item);

        }

    }



    if (needsCustomSort && result.items.size() > 1) {

        const bool asc = filter.sortAscending;

        std::sort(result.items.begin(), result.items.end(), [&](const DNItem* a, const DNItem* b) noexcept {

            int cmp = 0;

            switch (filter.sortBy) {

                case ItemSortColumn::Id:

                    cmp = (a->id < b->id) ? -1 : ((a->id > b->id) ? 1 : 0);

                    break;

                case ItemSortColumn::Name:

                    cmp = a->lowerName.compare(b->lowerName);

                    break;

                case ItemSortColumn::Rank:

                    cmp = (a->rank < b->rank) ? -1 : ((a->rank > b->rank) ? 1 : 0);

                    break;

                case ItemSortColumn::Level:

                    cmp = (a->levelLimit < b->levelLimit) ? -1 : ((a->levelLimit > b->levelLimit) ? 1 : 0);

                    break;

                case ItemSortColumn::Type:

                    if (a->rawType != b->rawType) cmp = (a->rawType < b->rawType) ? -1 : 1;

                    else cmp = (a->equipSubType < b->equipSubType) ? -1 : ((a->equipSubType > b->equipSubType) ? 1 : 0);

                    break;

                case ItemSortColumn::Table:

                    cmp = (a->tableIndex < b->tableIndex) ? -1 : ((a->tableIndex > b->tableIndex) ? 1 : 0);

                    break;

            }

            if (cmp == 0) return asc ? (a->id < b->id) : (a->id > b->id);

            return asc ? (cmp < 0) : (cmp > 0);

        });

        if (filter.limit > 0 && result.items.size() > filter.limit) {

            result.items.resize(filter.limit);

        }

    }

    return result;

}



ItemQueryResult ItemManager::Search(const std::string& keyword, int groupFilter, size_t limit) const {

    ItemSearchFilter f;

    f.keyword = keyword;

    f.groupFilter = groupFilter;

    f.limit = limit;

    return Search(f);

}



bool ItemManager::IsIdUsedAnywhere(int32_t id) const {

    if (id <= 0) return true;

    if (m_indexById.contains(id)) return true;

    if (m_weaponTableById.contains(id) || m_partsTableById.contains(id)) return true;

    for (const auto& t : m_itemTables) {

        if (t.HasRow(id)) return true;

    }

    return false;

}



int32_t ItemManager::SuggestNextItemId(int tableIndex, int32_t nearId) const {

    int32_t candidate = nearId > 0 ? nearId + 1 : 0;

    if (candidate <= 0) {

        int32_t maxId = 100000;

        if (tableIndex >= 0 && static_cast<size_t>(tableIndex) < m_itemTables.size()) {

            for (const auto& r : m_itemTables[static_cast<size_t>(tableIndex)].GetRows()) {

                if (r.rowId > maxId) maxId = r.rowId;

            }

        } else {

            for (const auto& item : m_items) {

                if (!item.deleted && item.id > maxId) maxId = item.id;

            }

        }

        candidate = maxId + 1;

    }

    while (IsIdUsedAnywhere(candidate)) ++candidate;

    return candidate;

}



void ItemManager::AdjustMidRef(uint32_t mid, int delta) noexcept {

    if (mid == 0 || delta == 0) return;

    if (delta > 0) {

        m_midRefCount[mid] += static_cast<uint32_t>(delta);

    } else {

        const auto it = m_midRefCount.find(mid);

        if (it != m_midRefCount.end()) {

            const auto sub = static_cast<uint32_t>(-delta);

            if (it->second <= sub) m_midRefCount.erase(it);

            else it->second -= sub;

        }

    }

}



size_t ItemManager::CountItemsUsingMid(uint32_t mid) const noexcept {

    if (mid == 0) return 0;

    const auto it = m_midRefCount.find(mid);

    return it != m_midRefCount.end() ? static_cast<size_t>(it->second) : 0;

}



std::vector<int32_t> ItemManager::GetModifiedItemIds() const {

    std::vector<int32_t> ids(m_modifiedItemIds.begin(), m_modifiedItemIds.end());

    std::sort(ids.begin(), ids.end());

    return ids;

}



void ItemManager::SetItemModifiedFlag(int32_t id, bool modified) {

    if (modified) m_modifiedItemIds.insert(id);

    else m_modifiedItemIds.erase(id);

}



uint32_t ItemManager::AllocateUIStringMid(const std::string& initialText) {

    const uint32_t mid = m_uiStrings.AllocateNextMid();

    m_uiStrings.SetString(mid, initialText);

    m_dirtyStrings = true;

    return mid;

}



void ItemManager::WriteItemRow(const DNItem& item) {

    if (item.tableIndex >= m_itemTables.size()) return;

    auto& t = m_itemTables[item.tableIndex];

    const auto& c = m_itemCols[item.tableIndex];

    const int32_t id = item.id;



    if (DNTRow* r = t.GetRow(id)) {

        auto seti = [&](int col, int32_t v) noexcept {

            if (col >= 0) t.SetInt(*r, static_cast<size_t>(col), v);

        };

        auto sets = [&](int col, std::string_view v) {

            if (col >= 0) t.SetString(*r, static_cast<size_t>(col), v);

        };



        seti(c.nameId, static_cast<int32_t>(item.nameId));

        seti(c.descId, static_cast<int32_t>(item.descId));

        sets(c.nameParam, item.nameParam);

        sets(c.descParam, item.descParam);

        seti(c.type, item.rawType);

        seti(c.rank, item.rank);

        seti(c.levelLimit, item.levelLimit);

        seti(c.heroLevelLimit, item.heroLevelLimit);

        sets(c.jobClass, item.jobClass);

        seti(c.overlap, item.overlapCount);

        seti(c.sell, item.sellAmount);

        seti(c.icon, item.iconIndex);

        seti(c.gearScore, item.gearScore);

        seti(c.typeParam1, item.typeParam1);

        seti(c.typeParam2, item.typeParam2);

        seti(c.typeParam3, item.typeParam3);

        seti(c.isCash, item.isCash ? 1 : 0);

        seti(c.ableStorage, item.ableStorage ? 1 : 0);

        seti(c.ableWStorage, item.ableWStorage ? 1 : 0);
        seti(c.dragonJewelType, item.dragonJewelType);
        seti(c.enchantId, item.enchantId);
        for (size_t i = 0; i < 10; ++i) {
            seti(c.potential[i], item.potentialIds[i]);
        }



        for (size_t i = 0; i < kStateSlotCount; ++i) {

            const auto& s = item.states[i];

            seti(c.state[i], s.type);

            if (c.prob[i] >= 0) t.SetFloat(*r, static_cast<size_t>(c.prob[i]), s.genProb);

            sets(c.minV[i], s.minValue);

            sets(c.maxV[i], s.maxValue);

        }

        m_dirtyItems[item.tableIndex] = true;

    }



    size_t detailIdx = 0;

    if (DNTTable* detail = FindMutableDetailTableForItem(item, &detailIdx)) {

        if (DNTRow* dRow = detail->GetRow(id)) {

            if (item.rawType == 0) {

                const auto& wc = m_weaponCols[detailIdx];

                if (wc.durability >= 0 && detail->GetInt(*dRow, static_cast<size_t>(wc.durability)) != item.durability) {

                    detail->SetInt(*dRow, static_cast<size_t>(wc.durability), item.durability);

                    m_dirtyWeapons[detailIdx] = true;

                }

                if (wc.equipType >= 0 && item.equipSubType >= 0 &&

                    detail->GetInt(*dRow, static_cast<size_t>(wc.equipType)) != item.equipSubType) {

                    detail->SetInt(*dRow, static_cast<size_t>(wc.equipType), item.equipSubType);

                    m_dirtyWeapons[detailIdx] = true;

                }

            } else if (item.rawType == 1) {

                const auto& pc = m_partsCols[detailIdx];

                if (pc.durability >= 0 && detail->GetInt(*dRow, static_cast<size_t>(pc.durability)) != item.durability) {

                    detail->SetInt(*dRow, static_cast<size_t>(pc.durability), item.durability);

                    m_dirtyParts[detailIdx] = true;

                }

                if (pc.parts >= 0 && item.equipSubType >= 0 &&

                    detail->GetInt(*dRow, static_cast<size_t>(pc.parts)) != item.equipSubType) {

                    detail->SetInt(*dRow, static_cast<size_t>(pc.parts), item.equipSubType);

                    m_dirtyParts[detailIdx] = true;

                }

            }

        }

    }

}



bool ItemManager::ApplyItem(const DNItem& edited) {

    const auto it = m_indexById.find(edited.id);

    if (it == m_indexById.end() || m_items[it->second].deleted) {

        LOG_WARNING(Channels::engine, "ApplyItem: unknown item id {}", edited.id);

        return false;

    }



    DNItem item = edited;

    item.tableIndex = m_items[it->second].tableIndex;

    item.rawType = m_items[it->second].rawType;

    item.deleted = false;



    if (item.nameId == 0 && !item.nameTemplate.empty()) {

        item.nameId = m_uiStrings.AllocateNextMid();

        m_uiStrings.SetString(item.nameId, item.nameTemplate);

        m_dirtyStrings = true;

    } else if (item.nameId > 0 && m_uiStrings.GetStringView(item.nameId) != item.nameTemplate) {

        m_uiStrings.SetString(item.nameId, item.nameTemplate);

        m_dirtyStrings = true;

    }



    if (item.descId == 0 && !item.descTemplate.empty()) {

        item.descId = m_uiStrings.AllocateNextMid();

        m_uiStrings.SetString(item.descId, item.descTemplate);

        m_dirtyStrings = true;

    } else if (item.descId > 0 && m_uiStrings.GetStringView(item.descId) != item.descTemplate) {

        m_uiStrings.SetString(item.descId, item.descTemplate);

        m_dirtyStrings = true;

    }



    const DNItem& prev = m_items[it->second];

    if (prev.nameId > 0) AdjustMidRef(prev.nameId, -1);

    if (prev.descId > 0 && prev.descId != prev.nameId) AdjustMidRef(prev.descId, -1);



    WriteItemRow(item);

    item.group = DeriveGroup(item.rawType, item.equipSubType);

    ResolveItemText(item);

    if (item.nameId > 0) AdjustMidRef(item.nameId, +1);

    if (item.descId > 0 && item.descId != item.nameId) AdjustMidRef(item.descId, +1);

    m_items[it->second] = std::move(item);

    m_modifiedItemIds.insert(edited.id);



    const DNItem& stored = m_items[it->second];

    LOG_INFO(Channels::engine, "Applied item #{} '{}' in {}", stored.id, stored.name, GetItemTableName(stored.tableIndex));

    return true;

}



int32_t ItemManager::DuplicateItem(int32_t sourceId, const std::string& newName, int targetTableIndex, int32_t requestedNewId) {

    const DNItem* src = GetItem(sourceId);

    if (!src) return 0;



    DNItem copy = *src;

    const size_t srcTableIdx = copy.tableIndex;

    const size_t dstTableIdx = (targetTableIndex >= 0 && static_cast<size_t>(targetTableIndex) < m_itemTables.size())

                                   ? static_cast<size_t>(targetTableIndex)

                                   : srcTableIdx;

    if (!m_itemTables[dstTableIdx].HasSchema()) return 0;



    const int32_t newId = (requestedNewId > 0 && !IsIdUsedAnywhere(requestedNewId))

                              ? requestedNewId

                              : SuggestNextItemId(static_cast<int>(dstTableIdx), dstTableIdx == srcTableIdx ? sourceId : 0);



    if (!m_itemTables[dstTableIdx].CloneRowFrom(m_itemTables[srcTableIdx], sourceId, newId)) {

        return 0;

    }

    m_dirtyItems[dstTableIdx] = true;



    if (copy.rawType == 0) {

        const auto wIt = m_weaponTableById.find(sourceId);

        if (wIt != m_weaponTableById.end()) {

            const int dstWIdx = (dstTableIdx == srcTableIdx && m_weaponTables[wIt->second].HasSchema())

                                    ? static_cast<int>(wIt->second)

                                    : FindMatchingDetailTableIndex(dstTableIdx, true);

            if (dstWIdx >= 0 && m_weaponTables[static_cast<size_t>(dstWIdx)].CloneRowFrom(m_weaponTables[wIt->second], sourceId, newId)) {

                m_weaponTableById[newId] = static_cast<uint16_t>(dstWIdx);

                m_dirtyWeapons[static_cast<size_t>(dstWIdx)] = true;

            }

        }

    } else if (copy.rawType == 1) {

        const auto pIt = m_partsTableById.find(sourceId);

        if (pIt != m_partsTableById.end()) {

            const int dstPIdx = (dstTableIdx == srcTableIdx && m_partsTables[pIt->second].HasSchema())

                                    ? static_cast<int>(pIt->second)

                                    : FindMatchingDetailTableIndex(dstTableIdx, false);

            if (dstPIdx >= 0 && m_partsTables[static_cast<size_t>(dstPIdx)].CloneRowFrom(m_partsTables[pIt->second], sourceId, newId)) {

                m_partsTableById[newId] = static_cast<uint16_t>(dstPIdx);

                m_dirtyParts[static_cast<size_t>(dstPIdx)] = true;

            }

        }

    }



    copy.id = newId;

    copy.tableIndex = static_cast<uint16_t>(dstTableIdx);

    copy.nameId = m_uiStrings.AllocateNextMid();

    copy.descId = m_uiStrings.AllocateNextMid();

    copy.nameParam.clear();

    copy.descParam.clear();

    copy.nameTemplate = newName.empty() ? std::format("{} (Copy)", src->name) : newName;

    copy.descTemplate = src->description;

    m_uiStrings.SetString(copy.nameId, copy.nameTemplate);

    m_uiStrings.SetString(copy.descId, copy.descTemplate);

    m_dirtyStrings = true;



    WriteItemRow(copy);

    ResolveItemText(copy);

    if (copy.nameId > 0) AdjustMidRef(copy.nameId, +1);

    if (copy.descId > 0 && copy.descId != copy.nameId) AdjustMidRef(copy.descId, +1);

    m_indexById[newId] = m_items.size();

    m_items.push_back(std::move(copy));

    m_modifiedItemIds.insert(newId);



    LOG_INFO(Channels::engine, "Duplicated item #{} -> #{} ('{}') into {}",

             sourceId, newId, m_items.back().name, GetItemTableName(dstTableIdx));

    return newId;

}



int32_t ItemManager::CreateNewItem(int targetTableIndex, int32_t rawType, int32_t equipSubType,

                                   const std::string& newName, int32_t requestedNewId) {

    if (m_itemTables.empty()) return 0;

    const size_t dstTableIdx = (targetTableIndex >= 0 && static_cast<size_t>(targetTableIndex) < m_itemTables.size())

                                   ? static_cast<size_t>(targetTableIndex)

                                   : 0;

    if (!m_itemTables[dstTableIdx].HasSchema()) return 0;



    const int32_t newId = (requestedNewId > 0 && !IsIdUsedAnywhere(requestedNewId))

                              ? requestedNewId

                              : SuggestNextItemId(static_cast<int>(dstTableIdx), 0);



    if (!m_itemTables[dstTableIdx].CreateDefaultRow(newId)) return 0;

    m_dirtyItems[dstTableIdx] = true;



    if (rawType == 0) {

        const int wIdx = FindMatchingDetailTableIndex(dstTableIdx, true);

        if (wIdx >= 0 && m_weaponTables[static_cast<size_t>(wIdx)].CreateDefaultRow(newId)) {

            const auto& wc = m_weaponCols[static_cast<size_t>(wIdx)];

            m_weaponTables[static_cast<size_t>(wIdx)].SetInt(newId, wc.equipType, equipSubType >= 0 ? equipSubType : 0);

            m_weaponTables[static_cast<size_t>(wIdx)].SetInt(newId, wc.durability, 100);

            m_weaponTableById[newId] = static_cast<uint16_t>(wIdx);

            m_dirtyWeapons[static_cast<size_t>(wIdx)] = true;

        }

    } else if (rawType == 1) {

        const int pIdx = FindMatchingDetailTableIndex(dstTableIdx, false);

        if (pIdx >= 0 && m_partsTables[static_cast<size_t>(pIdx)].CreateDefaultRow(newId)) {

            const auto& pc = m_partsCols[static_cast<size_t>(pIdx)];

            m_partsTables[static_cast<size_t>(pIdx)].SetInt(newId, pc.parts, equipSubType >= 0 ? equipSubType : 3);

            m_partsTables[static_cast<size_t>(pIdx)].SetInt(newId, pc.durability, 100);

            m_partsTableById[newId] = static_cast<uint16_t>(pIdx);

            m_dirtyParts[static_cast<size_t>(pIdx)] = true;

        }

    }



    DNItem item;

    item.id = newId;

    item.tableIndex = static_cast<uint16_t>(dstTableIdx);

    item.rawType = rawType;

    item.equipSubType = (rawType == 0) ? (equipSubType >= 0 ? equipSubType : 0)

                                       : ((rawType == 1) ? (equipSubType >= 0 ? equipSubType : 3) : -1);

    item.durability = (rawType == 0 || rawType == 1) ? 100 : 0;

    item.nameId = m_uiStrings.AllocateNextMid();

    item.descId = m_uiStrings.AllocateNextMid();

    item.nameTemplate = newName.empty() ? std::format("New Item {}", newId) : newName;

    item.descTemplate.clear();

    m_uiStrings.SetString(item.nameId, item.nameTemplate);

    m_uiStrings.SetString(item.descId, item.descTemplate);

    m_dirtyStrings = true;



    WriteItemRow(item);

    ReadItemRow(newId, static_cast<uint16_t>(dstTableIdx), item);

    if (item.nameId > 0) AdjustMidRef(item.nameId, +1);

    if (item.descId > 0 && item.descId != item.nameId) AdjustMidRef(item.descId, +1);

    m_indexById[newId] = m_items.size();

    m_items.push_back(std::move(item));

    m_modifiedItemIds.insert(newId);



    LOG_INFO(Channels::engine, "Created new item #{} ('{}', type={}) in {}",

             newId, m_items.back().name, rawType, GetItemTableName(dstTableIdx));

    return newId;

}



bool ItemManager::DeleteItem(int32_t id) {

    const auto it = m_indexById.find(id);

    if (it == m_indexById.end()) return false;



    DNItem& item = m_items[it->second];

    if (item.deleted) return false;



    if (item.nameId > 0) AdjustMidRef(item.nameId, -1);

    if (item.descId > 0 && item.descId != item.nameId) AdjustMidRef(item.descId, -1);



    if (item.tableIndex < m_itemTables.size()) {

        if (m_itemTables[item.tableIndex].DeleteRow(id)) {

            m_dirtyItems[item.tableIndex] = true;

        }

    }



    const auto wIt = m_weaponTableById.find(id);

    if (wIt != m_weaponTableById.end()) {

        if (m_weaponTables[wIt->second].DeleteRow(id)) {

            m_dirtyWeapons[wIt->second] = true;

        }

        m_weaponTableById.erase(wIt);

    }



    const auto pIt = m_partsTableById.find(id);

    if (pIt != m_partsTableById.end()) {

        if (m_partsTables[pIt->second].DeleteRow(id)) {

            m_dirtyParts[pIt->second] = true;

        }

        m_partsTableById.erase(pIt);

    }



    item.deleted = true;

    m_indexById.erase(it);

    m_modifiedItemIds.erase(id);

    LOG_INFO(Channels::engine, "Deleted item #{} from {}", id, GetItemTableName(item.tableIndex));

    return true;

}



bool ItemManager::SetRawItemCell(int32_t itemId, bool detailTable, size_t colIdx, std::string_view textValue) {

    const auto it = m_indexById.find(itemId);

    if (it == m_indexById.end() || m_items[it->second].deleted) return false;



    DNItem& item = m_items[it->second];

    if (item.nameId > 0) AdjustMidRef(item.nameId, -1);

    if (item.descId > 0 && item.descId != item.nameId) AdjustMidRef(item.descId, -1);



    if (!detailTable) {

        if (item.tableIndex >= m_itemTables.size()) return false;

        auto& tbl = m_itemTables[item.tableIndex];

        if (!tbl.ParseAndSetCell(itemId, colIdx, textValue)) {

            if (item.nameId > 0) AdjustMidRef(item.nameId, +1);

            if (item.descId > 0 && item.descId != item.nameId) AdjustMidRef(item.descId, +1);

            return false;

        }

        m_dirtyItems[item.tableIndex] = true;

    } else {

        size_t detailIdx = 0;

        DNTTable* detail = FindMutableDetailTableForItem(item, &detailIdx);

        if (!detail || !detail->ParseAndSetCell(itemId, colIdx, textValue)) {

            if (item.nameId > 0) AdjustMidRef(item.nameId, +1);

            if (item.descId > 0 && item.descId != item.nameId) AdjustMidRef(item.descId, +1);

            return false;

        }

        if (item.rawType == 0) {

            m_dirtyWeapons[detailIdx] = true;

        } else if (item.rawType == 1) {

            m_dirtyParts[detailIdx] = true;

        }

    }



    ReadItemRow(itemId, item.tableIndex, item);

    if (item.nameId > 0) AdjustMidRef(item.nameId, +1);

    if (item.descId > 0 && item.descId != item.nameId) AdjustMidRef(item.descId, +1);

    m_modifiedItemIds.insert(itemId);

    return true;

}



bool ItemManager::HasUnsavedChanges() const noexcept {
    if (m_dirtyDragonJewelTable) return true;
    if (m_dirtyStrings) return true;

    for (const auto& p : m_charmTables) {

        if (p.dirtyItem || p.dirtyProb) return true;

    }

    auto anyTrue = [](const std::vector<bool>& v) noexcept {

        return std::any_of(v.begin(), v.end(), [](bool b) noexcept { return b; });

    };

    return anyTrue(m_dirtyItems) || anyTrue(m_dirtyWeapons) || anyTrue(m_dirtyParts);

}



size_t ItemManager::GetDirtyTableCount() const noexcept {
    size_t count = (m_dirtyStrings ? 1 : 0) + (m_dirtyDragonJewelTable ? 1 : 0);

    for (bool b : m_dirtyItems) if (b) ++count;

    for (bool b : m_dirtyWeapons) if (b) ++count;

    for (bool b : m_dirtyParts) if (b) ++count;

    for (const auto& p : m_charmTables) {

        if (p.dirtyItem) ++count;

        if (p.dirtyProb) ++count;

    }

    return count;

}



bool ItemManager::CommitToDisk(std::string& report) {

    int written = 0;

    bool ok = true;



    auto writeTable = [&](DNTTable& table) -> bool {

        const auto& path = table.GetPath();

        if (path.empty()) return true;

        if (!BackupOnce(path)) {

            report += std::format("Backup failed for {}\r\n", table.GetFileName());

            ok = false;

            return false;

        }

        auto tmp = path;

        tmp += ".tmp";

        if (!table.SaveToFile(tmp) || !ReplaceFile(tmp, path)) {

            report += std::format("Write failed for {}\r\n", table.GetFileName());

            ok = false;

            return false;

        }

        ++written;

        return true;

    };



    for (size_t i = 0; i < m_itemTables.size(); ++i) {

        if (m_dirtyItems[i] && writeTable(m_itemTables[i])) m_dirtyItems[i] = false;

    }

    for (size_t i = 0; i < m_weaponTables.size(); ++i) {

        if (m_dirtyWeapons[i] && writeTable(m_weaponTables[i])) m_dirtyWeapons[i] = false;

    }

    for (size_t i = 0; i < m_partsTables.size(); ++i) {

        if (m_dirtyParts[i] && writeTable(m_partsTables[i])) m_dirtyParts[i] = false;

    }

    if (m_dirtyDragonJewelTable && writeTable(m_dragonJewelTable)) {
        m_dirtyDragonJewelTable = false;
    }



    if (m_dirtyStrings) {

        const auto& path = m_uiStrings.GetPath();

        if (BackupOnce(path)) {

            auto tmp = path;

            tmp += ".tmp";

            if (m_uiStrings.SaveToFile(tmp) && ReplaceFile(tmp, path)) {

                ++written;

                m_dirtyStrings = false;

            } else {

                report += "Write failed for uistring.xml\r\n";

                ok = false;

            }

        } else {

            report += "Backup failed for uistring.xml\r\n";

            ok = false;

        }

    }



    if (ok) {

        m_modifiedItemIds.clear();

    }

    report += std::format("{} file(s) written{}", written, ok ? "." : " (with errors).");

    return ok;

}



bool ItemManager::SaveModifiedToFolder(const std::filesystem::path& outputFolder, std::string& report) {

    std::error_code ec;

    std::filesystem::create_directories(outputFolder, ec);

    if (ec) {

        report = "Failed to create destination folder: " + outputFolder.string();

        return false;

    }



    report = "Saving modified files to: " + outputFolder.string() + "\n";

    size_t written = 0;

    bool ok = true;



    auto writeTableToFolder = [&](const DNTTable& table) -> bool {

        const auto& path = table.GetPath();

        if (path.empty()) return true;



        std::filesystem::path rel;

        if (!m_resourcePath.empty()) {

            rel = std::filesystem::relative(path, m_resourcePath, ec);

            if (ec || rel.empty()) rel = path.filename();

        } else {

            rel = path.filename();

        }



        const auto dest = outputFolder / rel;

        std::filesystem::create_directories(dest.parent_path(), ec);

        if (!table.SaveToFile(dest, false)) {

            report += "Write failed for " + dest.string() + "\n";

            ok = false;

            return false;

        }

        report += "Saved: " + rel.string() + "\n";

        ++written;

        return true;

    };



    for (size_t i = 0; i < m_itemTables.size(); ++i) {

        if (m_dirtyItems[i]) {

            if (writeTableToFolder(m_itemTables[i])) m_dirtyItems[i] = false;

        }

    }

    for (size_t i = 0; i < m_weaponTables.size(); ++i) {

        if (m_dirtyWeapons[i]) {

            if (writeTableToFolder(m_weaponTables[i])) m_dirtyWeapons[i] = false;

        }

    }

    for (size_t i = 0; i < m_partsTables.size(); ++i) {

        if (m_dirtyParts[i]) {

            if (writeTableToFolder(m_partsTables[i])) m_dirtyParts[i] = false;

        }

    }

    for (auto& pair : m_charmTables) {

        if (pair.dirtyItem && writeTableToFolder(pair.itemTable)) {

            pair.dirtyItem = false;

            ++written;

        }

        if (pair.dirtyProb && writeTableToFolder(pair.probTable)) {

            pair.dirtyProb = false;

            ++written;

        }

    }

    if (m_dirtyDragonJewelTable && writeTableToFolder(m_dragonJewelTable)) {
        m_dirtyDragonJewelTable = false;
    }



    if (m_dirtyStrings) {

        const auto& path = m_uiStrings.GetPath();

        std::filesystem::path rel;

        if (!m_resourcePath.empty()) {

            rel = std::filesystem::relative(path, m_resourcePath, ec);

            if (ec || rel.empty()) rel = path.filename();

        } else {

            rel = path.filename();

        }

        const auto dest = outputFolder / rel;

        std::filesystem::create_directories(dest.parent_path(), ec);

        if (m_uiStrings.SaveToFile(dest, false)) {

            report += "Saved: " + rel.string() + "\n";

            ++written;

            m_dirtyStrings = false;

        } else {

            report += "Write failed for " + dest.string() + "\n";

            ok = false;

        }

    }



    if (ok) {

        m_modifiedItemIds.clear();

    }

    report += "\nTotal " + std::to_string(written) + " file(s) saved to: " + outputFolder.string();

    return ok;

}



bool ItemManager::VerifyRoundTrip(std::string& report) const {

    namespace fs = std::filesystem;

    const auto tStart = std::chrono::steady_clock::now();



    std::error_code ec;

    const fs::path dir = fs::temp_directory_path(ec) / "dnitemmaker_roundtrip";

    fs::create_directories(dir, ec);



    struct VerifyTask {

        const DNTTable* dnt = nullptr;

        bool isUiString = false;

        std::string label;

    };



    std::vector<VerifyTask> tasks;

    tasks.reserve(m_itemTables.size() + m_weaponTables.size() + m_partsTables.size() + 3);

    auto addTable = [&](const DNTTable& t) {

        if (!t.GetPath().empty()) tasks.push_back(VerifyTask{ &t, false, t.GetFileName() });

    };

    for (const auto& t : m_itemTables) addTable(t);

    for (const auto& t : m_weaponTables) addTable(t);

    for (const auto& t : m_partsTables) addTable(t);

    addTable(m_fileTable);

    addTable(m_jobTable);
    addTable(m_dragonJewelTable);
    addTable(m_dragonJewelSlotTable);
    for (const auto& t : m_setItemTables) addTable(t);
    for (const auto& t : m_enchantTables) addTable(t);
    for (const auto& t : m_potentialTables) addTable(t);

    if (!m_uiStrings.GetPath().empty()) {

        tasks.push_back(VerifyTask{ nullptr, true, "uistring.xml" });

    }



    auto readAll = [](const fs::path& p) {

        std::ifstream f(p, std::ios::binary | std::ios::ate);

        if (!f.is_open()) return std::vector<char>{};

        const auto szSigned = f.tellg();

        if (szSigned <= 0) return std::vector<char>{};

        const auto sz = static_cast<size_t>(szSigned);

        f.seekg(0, std::ios::beg);

        std::vector<char> buf(sz);

        f.read(buf.data(), static_cast<std::streamsize>(sz));

        return buf;

    };



    std::vector<std::string> lines(tasks.size());

    std::vector<uint8_t> passedFlags(tasks.size(), 0);



    ParallelFor(tasks.size(), [&](size_t idx) {

        const VerifyTask& task = tasks[idx];

        const fs::path outPath = dir / std::format("{}_{}", idx, task.label);

        const fs::path& origPath = task.isUiString ? m_uiStrings.GetPath() : task.dnt->GetPath();



        const bool saved = task.isUiString

                               ? m_uiStrings.SaveToFile(outPath, /*quiet=*/true)

                               : task.dnt->SaveToFile(outPath, /*quiet=*/true);

        if (!saved) {

            lines[idx] = std::format("[FAIL] {} could not be written\r\n", task.label);

            return;

        }



        const auto a = readAll(origPath);

        const auto b = readAll(outPath);

        if (a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size()) == 0)) {

            passedFlags[idx] = 1;

            lines[idx] = std::format("[OK]   {} identical ({} bytes)\r\n", task.label, a.size());

        } else {

            const auto mm = std::mismatch(a.begin(), a.end(), b.begin(), b.end());

            lines[idx] = std::format("[FAIL] {} differs at byte {} (sizes {} vs {})\r\n", task.label,

                                     static_cast<size_t>(mm.first - a.begin()), a.size(), b.size());

        }

    });



    size_t passedCount = 0;

    std::string body;

    body.reserve(tasks.size() * 64);

    for (size_t i = 0; i < tasks.size(); ++i) {

        if (passedFlags[i]) ++passedCount;

        body.append(lines[i]);

    }



    const auto elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tStart).count();

    report = std::format("Round-Trip Summary: {}/{} files byte-identical (verified in {:.1f} ms).\r\n\r\n{}",

                         passedCount, tasks.size(), elapsedMs, body);



    fs::remove_all(dir, ec);

    return passedCount == tasks.size();

}



std::filesystem::path ItemManager::IconAtlasPath(int32_t iconIndex) const {

    if (iconIndex < 0 || m_resourcePath.empty()) return {};

    return m_resourcePath / "ui" / "mainbar" / std::format("itemicon{:02d}.dds", iconIndex / 200 + 1);

}



void ItemManager::IndexModels() const {

    if (m_modelIndexBuilt.load(std::memory_order_acquire) || m_resourcePath.empty()) return;

    std::scoped_lock lock(m_modelMutex);

    if (m_modelIndexBuilt.load(std::memory_order_relaxed) || m_resourcePath.empty()) return;



    const auto tStart = std::chrono::steady_clock::now();



    constexpr const char* kFolders[3] = { "weapon", "char", "item" };

    std::array<std::vector<std::pair<std::string, std::filesystem::path>>, 3> perFolder;



    auto scanFolder = [&](size_t idx) {

        std::error_code ec;

        const auto dir = m_resourcePath / kFolders[idx];

        if (!std::filesystem::exists(dir, ec)) return;



        auto& outVec = perFolder[idx];

        outVec.reserve(idx == 1 ? 96000 : 32000);



        for (auto it = std::filesystem::recursive_directory_iterator(

                 dir, std::filesystem::directory_options::skip_permission_denied, ec);

             it != std::filesystem::recursive_directory_iterator();

             it.increment(ec)) {

            if (ec) {

                ec.clear();

                continue;

            }

            if (!it->is_regular_file(ec)) continue;



            const auto& p = it->path();

            const auto& native = p.native();

            if (native.size() < 5) continue;



                        const auto* ext = native.data() + native.size() - 4;

            if (ext[0] != '.') continue;

            const auto e1 = (ext[1] >= 'A' && ext[1] <= 'Z') ? (ext[1] + 32) : ext[1];

            const auto e2 = (ext[2] >= 'A' && ext[2] <= 'Z') ? (ext[2] + 32) : ext[2];

            const auto e3 = (ext[3] >= 'A' && ext[3] <= 'Z') ? (ext[3] + 32) : ext[3];

            const bool match = (e1 == 'd' && e2 == 'd' && e3 == 's') ||

                               (e1 == 'm' && e2 == 's' && e3 == 'h') ||

                               (e1 == 's' && e2 == 'k' && e3 == 'n');

            if (!match) continue;



                        size_t slashPos = native.size();

            while (slashPos > 0 && native[slashPos - 1] != '\\' && native[slashPos - 1] != '/') {

                --slashPos;

            }

            const size_t nameLen = native.size() - slashPos;

            bool pureAscii = true;

            for (size_t k = slashPos; k < native.size(); ++k) {

                if (static_cast<uint32_t>(native[k]) >= 0x80u) {

                    pureAscii = false;

                    break;

                }

            }



            std::string key;

            if (pureAscii) {

                key.resize(nameLen);

                for (size_t k = 0; k < nameLen; ++k) {

                    key[k] = AsciiLower(static_cast<char>(native[slashPos + k]));

                }

            } else {

                const auto u8 = p.filename().u8string();

                key = ToLower(std::string_view(reinterpret_cast<const char*>(u8.data()), u8.size()));

            }

            outVec.emplace_back(std::move(key), p);

        }

    };



    {

        std::jthread t0([&] { scanFolder(0); });

        std::jthread t1([&] { scanFolder(1); });

        scanFolder(2);

    }



    const size_t totalFiles = perFolder[0].size() + perFolder[1].size() + perFolder[2].size();

    m_modelIndex.reserve(totalFiles + 64);

    for (auto& vec : perFolder) {

        for (auto& kv : vec) {

            m_modelIndex.try_emplace(std::move(kv.first), std::move(kv.second));

        }

    }



    m_modelIndexBuilt.store(true, std::memory_order_release);

    const auto elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tStart).count();

    LOG_INFO(Channels::graphics, "Indexed {} model/texture files across weapon/char/item folders in {:.1f} ms",

             m_modelIndex.size(), elapsedMs);

}



std::filesystem::path ItemManager::FindSkinPath(const DNItem& item) const {

    int32_t fileId = 0;



    if (item.rawType == 0) {

        const auto wIt = m_weaponTableById.find(item.id);

        if (wIt != m_weaponTableById.end()) {

            const auto& t = m_weaponTables[wIt->second];

            const auto& wc = m_weaponCols[wIt->second];

            if (const auto* row = t.GetRow(item.id)) {

                for (int col : wc.skinNames) {

                    if (col >= 0) {

                        const int32_t v = t.GetInt(*row, static_cast<size_t>(col));

                        if (v > 0) {

                            fileId = v;

                            break;

                        }

                    }

                }

            }

        }

    } else if (item.rawType == 1) {

        const auto pIt = m_partsTableById.find(item.id);

        if (pIt != m_partsTableById.end()) {

            const auto& t = m_partsTables[pIt->second];

            const auto& pc = m_partsCols[pIt->second];

            if (const auto* row = t.GetRow(item.id)) {

                for (int col : pc.playerSkins) {

                    if (col >= 0) {

                        const int32_t v = t.GetInt(*row, static_cast<size_t>(col));

                        if (v > 0) {

                            fileId = v;

                            break;

                        }

                    }

                }

                if (fileId <= 0 && pc.skinId >= 0) {

                    const auto it = m_skinCombine.find(t.GetInt(*row, static_cast<size_t>(pc.skinId)));

                    if (it != m_skinCombine.end()) fileId = it->second;

                }

            }

        }

    }

    if (fileId <= 0 || m_fileNameCol < 0) return {};



    const auto* fileRow = m_fileTable.GetRow(fileId);

    if (!fileRow) return {};

    const std::string& skinName = m_fileTable.GetString(*fileRow, static_cast<size_t>(m_fileNameCol));

    if (skinName.empty()) return {};



    return FindModelResource(skinName);

}



std::filesystem::path ItemManager::FindModelResource(const std::string& fileName, const std::filesystem::path& nearFile) const {

    if (fileName.empty()) return {};



    std::string_view sv(fileName);

    const size_t slash = sv.find_last_of("\\/");

    if (slash != std::string_view::npos) sv.remove_prefix(slash + 1);

    if (sv.empty()) return {};



    const std::string base(sv);

    std::error_code ec;



        if (!nearFile.empty()) {

        const auto candidate = nearFile.parent_path() / base;

        if (std::filesystem::exists(candidate, ec)) return candidate;

    }



        if (!m_resourcePath.empty()) {

        for (const char* sub : { "weapon", "char", "item" }) {

            const auto direct = m_resourcePath / sub / base;

            if (std::filesystem::exists(direct, ec)) return direct;

        }

    }



        if (!m_modelIndexBuilt.load(std::memory_order_acquire)) {

        std::unique_lock<std::mutex> tryLock(m_modelMutex, std::try_to_lock);

        if (!tryLock.owns_lock()) {

            return {};

        }

        tryLock.unlock();

        IndexModels();

    }



    const auto it = m_modelIndex.find(ToLower(sv));

    return it != m_modelIndex.end() ? it->second : std::filesystem::path{};

}



std::filesystem::path ItemManager::FindMeshPath(const DNItem& item) const {

    const auto skinPath = FindSkinPath(item);

    if (skinPath.empty()) return {};



    std::ifstream skn(skinPath, std::ios::binary);

    char header[512] = {};

    if (!skn.read(header, sizeof(header))) return {};

    if (std::strncmp(header, "Eternity Engine Skin File", 25) != 0) return {};

    const std::string meshName(header + 256, strnlen(header + 256, 256));

    return FindModelResource(meshName, skinPath);

}



bool ItemManager::CopyVisualFromItem(int32_t targetId, int32_t sourceVisualItemId, bool copyIcon, bool copy3DSkin) {

    if (!copyIcon && !copy3DSkin) return false;

    const auto dstIt = m_indexById.find(targetId);

    const DNItem* src = GetItem(sourceVisualItemId);

    if (dstIt == m_indexById.end() || !src || m_items[dstIt->second].deleted) return false;



    DNItem& dst = m_items[dstIt->second];

    bool changed = false;



    if (copyIcon) {

        dst.iconIndex = src->iconIndex;

        WriteItemRow(dst);

        changed = true;

    }



    if (copy3DSkin && dst.rawType == src->rawType) {

        if (dst.rawType == 0) {

            const auto srcWIt = m_weaponTableById.find(src->id);

            if (srcWIt != m_weaponTableById.end()) {

                const DNTTable& srcTbl = m_weaponTables[srcWIt->second];

                const WeaponCols& srcWc = m_weaponCols[srcWIt->second];

                const DNTRow* srcRow = srcTbl.GetRow(src->id);

                if (srcRow) {

                    auto dstWIt = m_weaponTableById.find(dst.id);

                    if (dstWIt == m_weaponTableById.end()) {

                        const int newWIdx = FindMatchingDetailTableIndex(dst.tableIndex, true);

                        if (newWIdx >= 0 && m_weaponTables[static_cast<size_t>(newWIdx)].CreateDefaultRow(dst.id)) {

                            const auto& wc = m_weaponCols[static_cast<size_t>(newWIdx)];

                            m_weaponTables[static_cast<size_t>(newWIdx)].SetInt(dst.id, wc.equipType, dst.equipSubType >= 0 ? dst.equipSubType : src->equipSubType);

                            m_weaponTables[static_cast<size_t>(newWIdx)].SetInt(dst.id, wc.durability, dst.durability > 0 ? dst.durability : 100);

                            m_weaponTableById[dst.id] = static_cast<uint16_t>(newWIdx);

                            m_dirtyWeapons[static_cast<size_t>(newWIdx)] = true;

                            dstWIt = m_weaponTableById.find(dst.id);

                        }

                    }

                    if (dstWIt != m_weaponTableById.end()) {

                        DNTTable& dstTbl = m_weaponTables[dstWIt->second];

                        const WeaponCols& dstWc = m_weaponCols[dstWIt->second];

                        if (DNTRow* dstRow = dstTbl.GetRow(dst.id)) {

                            for (size_t k = 0; k < dstWc.skinNames.size(); ++k) {

                                if (dstWc.skinNames[k] >= 0 && srcWc.skinNames[k] >= 0) {

                                    const int32_t v = srcTbl.GetInt(*srcRow, static_cast<size_t>(srcWc.skinNames[k]));

                                    dstTbl.SetInt(*dstRow, static_cast<size_t>(dstWc.skinNames[k]), v);

                                }

                            }

                            m_dirtyWeapons[dstWIt->second] = true;

                            changed = true;

                        }

                    }

                }

            }

        } else if (dst.rawType == 1) {

            const auto srcPIt = m_partsTableById.find(src->id);

            if (srcPIt != m_partsTableById.end()) {

                const DNTTable& srcTbl = m_partsTables[srcPIt->second];

                const PartsCols& srcPc = m_partsCols[srcPIt->second];

                const DNTRow* srcRow = srcTbl.GetRow(src->id);

                if (srcRow) {

                    auto dstPIt = m_partsTableById.find(dst.id);

                    if (dstPIt == m_partsTableById.end()) {

                        const int newPIdx = FindMatchingDetailTableIndex(dst.tableIndex, false);

                        if (newPIdx >= 0 && m_partsTables[static_cast<size_t>(newPIdx)].CreateDefaultRow(dst.id)) {

                            const auto& pc = m_partsCols[static_cast<size_t>(newPIdx)];

                            m_partsTables[static_cast<size_t>(newPIdx)].SetInt(dst.id, pc.parts, dst.equipSubType >= 0 ? dst.equipSubType : src->equipSubType);

                            m_partsTables[static_cast<size_t>(newPIdx)].SetInt(dst.id, pc.durability, dst.durability > 0 ? dst.durability : 100);

                            m_partsTableById[dst.id] = static_cast<uint16_t>(newPIdx);

                            m_dirtyParts[static_cast<size_t>(newPIdx)] = true;

                            dstPIt = m_partsTableById.find(dst.id);

                        }

                    }

                    if (dstPIt != m_partsTableById.end()) {

                        DNTTable& dstTbl = m_partsTables[dstPIt->second];

                        const PartsCols& dstPc = m_partsCols[dstPIt->second];

                        if (DNTRow* dstRow = dstTbl.GetRow(dst.id)) {

                            if (dstPc.skinId >= 0 && srcPc.skinId >= 0) {

                                dstTbl.SetInt(*dstRow, static_cast<size_t>(dstPc.skinId),

                                              srcTbl.GetInt(*srcRow, static_cast<size_t>(srcPc.skinId)));

                            }

                            for (size_t k = 0; k < dstPc.playerSkins.size(); ++k) {

                                if (dstPc.playerSkins[k] >= 0 && srcPc.playerSkins[k] >= 0) {

                                    dstTbl.SetInt(*dstRow, static_cast<size_t>(dstPc.playerSkins[k]),

                                                  srcTbl.GetInt(*srcRow, static_cast<size_t>(srcPc.playerSkins[k])));

                                }

                            }

                            m_dirtyParts[dstPIt->second] = true;

                            changed = true;

                        }

                    }

                }

            }

        }

    }



    if (changed) {

        m_modifiedItemIds.insert(targetId);

        LOG_INFO(Channels::engine, "Copied visual assets from #{} -> #{} (icon={}, skin={})",

                 sourceVisualItemId, targetId, copyIcon, copy3DSkin);

    }

    return changed;

}



bool ItemManager::CloneDetailRowFromItem(int32_t targetId, int32_t sourceItemId) {

    if (targetId == sourceItemId) return true;

    const auto dstIt = m_indexById.find(targetId);

    const DNItem* src = GetItem(sourceItemId);

    if (dstIt == m_indexById.end() || !src || m_items[dstIt->second].deleted) return false;



    DNItem& dst = m_items[dstIt->second];

    if (dst.rawType != src->rawType || (dst.rawType != 0 && dst.rawType != 1)) return false;



    if (dst.rawType == 0) {

        const auto srcWIt = m_weaponTableById.find(sourceItemId);

        if (srcWIt == m_weaponTableById.end()) return false;



        int dstWIdx = -1;

        const auto existingWIt = m_weaponTableById.find(targetId);

        if (existingWIt != m_weaponTableById.end()) {

            dstWIdx = static_cast<int>(existingWIt->second);

            m_weaponTables[static_cast<size_t>(dstWIdx)].DeleteRow(targetId);

            m_dirtyWeapons[static_cast<size_t>(dstWIdx)] = true;

        } else {

            dstWIdx = FindMatchingDetailTableIndex(dst.tableIndex, true);

        }

        if (dstWIdx < 0) return false;



        if (!m_weaponTables[static_cast<size_t>(dstWIdx)].CloneRowFrom(m_weaponTables[srcWIt->second], sourceItemId, targetId)) {

            return false;

        }

        m_weaponTableById[targetId] = static_cast<uint16_t>(dstWIdx);

        m_dirtyWeapons[static_cast<size_t>(dstWIdx)] = true;

    } else {

        const auto srcPIt = m_partsTableById.find(sourceItemId);

        if (srcPIt == m_partsTableById.end()) return false;



        int dstPIdx = -1;

        const auto existingPIt = m_partsTableById.find(targetId);

        if (existingPIt != m_partsTableById.end()) {

            dstPIdx = static_cast<int>(existingPIt->second);

            m_partsTables[static_cast<size_t>(dstPIdx)].DeleteRow(targetId);

            m_dirtyParts[static_cast<size_t>(dstPIdx)] = true;

        } else {

            dstPIdx = FindMatchingDetailTableIndex(dst.tableIndex, false);

        }

        if (dstPIdx < 0) return false;



        if (!m_partsTables[static_cast<size_t>(dstPIdx)].CloneRowFrom(m_partsTables[srcPIt->second], sourceItemId, targetId)) {

            return false;

        }

        m_partsTableById[targetId] = static_cast<uint16_t>(dstPIdx);

        m_dirtyParts[static_cast<size_t>(dstPIdx)] = true;

    }



    if (dst.nameId > 0) AdjustMidRef(dst.nameId, -1);

    if (dst.descId > 0 && dst.descId != dst.nameId) AdjustMidRef(dst.descId, -1);

    ReadItemRow(targetId, dst.tableIndex, dst);

    if (dst.nameId > 0) AdjustMidRef(dst.nameId, +1);

    if (dst.descId > 0 && dst.descId != dst.nameId) AdjustMidRef(dst.descId, +1);

    m_modifiedItemIds.insert(targetId);



    LOG_INFO(Channels::engine, "Cloned equipment detail row from #{} -> #{}", sourceItemId, targetId);

    return true;

}



namespace {



constexpr bool IsRatioStateType(int32_t stateType) noexcept {

    return (stateType >= 16 && stateType <= 23) || (stateType >= 50 && stateType <= 81);

}



double ParseStatNumber(std::string_view sv) noexcept {

    sv = Trim(sv);

    if (sv.empty()) return 0.0;

    double val = 0.0;

    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);

    return ec == std::errc{} ? val : 0.0;

}



std::string FormatStatValue(int32_t stateType, double val, bool forceDecimal = false) {

    if (std::fabs(val) < 1e-7) return "0";

    if (forceDecimal || IsRatioStateType(stateType)) {

        char buf[48];

        const auto res = std::to_chars(buf, buf + sizeof(buf), std::round(val * 10000.0) / 10000.0,

                                       std::chars_format::fixed, 4);

        if (res.ec == std::errc{}) {

            std::string s(buf, res.ptr);

            if (s.find('.') != std::string::npos) {

                while (!s.empty() && s.back() == '0') s.pop_back();

                if (!s.empty() && s.back() == '.') s.pop_back();

            }

            return s.empty() ? "0" : s;

        }

    }

    return std::to_string(static_cast<int64_t>(std::llround(val)));

}



constexpr double RankPowerMultiplier(int32_t rank) noexcept {

    switch (rank) {

        case 0: return 1.00; // Normal

        case 1: return 1.25; // Magic

        case 2: return 1.60; // Rare

        case 3: return 2.15; // Epic

        case 4: return 2.95; // Unique

        case 5: return 4.10; // Heroic / Legendary

        case 6: return 5.50; // Named / Ancient

        default: return 1.00;

    }

}



constexpr double EquipSlotBudgetWeight(const DNItem& item) noexcept {

    if (item.rawType == 0) {

        // Main weapon vs offhand/secondary weapon

        if (item.equipSubType == 14 || item.equipSubType == 15 || item.equipSubType == 20 ||

            item.equipSubType == 21 || item.equipSubType == 23 || item.equipSubType == 27) {

            return 0.78;

        }

        return 1.35;

    }

    if (item.rawType == 1) {

        switch (item.equipSubType) {

            case 3: return 1.00; // Upper Body

            case 4: return 0.90; // Lower Body

            case 2: return 0.78; // Helmet

            case 5: return 0.65; // Gloves

            case 6: return 0.65; // Boots

            case 7: return 0.62; // Necklace

            case 8: return 0.58; // Earrings

            case 9: return 0.52; // Ring

            default: return 0.75;

        }

    }

    return 0.55; // Plates, Talismans, Dragon Gems, etc.

}



} // namespace



int32_t ItemManager::ComputeSuggestedGearScore(const DNItem& item) noexcept {

    const int32_t lv = std::clamp(item.levelLimit, 1, 99);

    const int32_t heroBoost = std::max(0, item.heroLevelLimit) * 3;

    const double baseCurve = (24.0 + static_cast<double>(lv) * 7.5 + static_cast<double>(lv * lv) * 0.34 + heroBoost) *

                             RankPowerMultiplier(item.rank) * EquipSlotBudgetWeight(item);



    double statBonus = 0.0;

    for (const auto& st : item.states) {

        if (st.type < 0) continue;

        const double mx = std::max(ParseStatNumber(st.minValue), ParseStatNumber(st.maxValue));

        if (mx <= 0.0) continue;

        if (IsRatioStateType(st.type)) {

            statBonus += mx * 1600.0;

        } else if (st.type == 25 || st.type == 26) {

            statBonus += mx * 0.008;

        } else if (st.type == 29) {

            statBonus += mx * 0.55;

        } else if (st.type >= 4 && st.type <= 7) {

            statBonus += mx * 0.14;

        } else {

            statBonus += mx * 0.10;

        }

    }



    const int64_t gs = std::llround(baseCurve + statBonus * 0.35);

    return static_cast<int32_t>(std::clamp<int64_t>(gs, 1, 999999));

}



int32_t ItemManager::ComputeSuggestedSellAmount(const DNItem& item) noexcept {

    if (item.isCash) return 0;

    const int32_t lv = std::clamp(item.levelLimit, 1, 99);

    const double rankEconomy = 1.0 + static_cast<double>(std::clamp(item.rank, 0, 6)) * 0.85;

    double rawCopper = (15.0 + static_cast<double>(lv * lv) * 1.45) * rankEconomy;

    if (item.rawType == 0 || item.rawType == 1) {

        rawCopper *= (1.35 * EquipSlotBudgetWeight(item));

    } else if (item.overlapCount > 1) {

        rawCopper *= 0.18;

    }

    // Round cleanly to nearest 5 copper (or 100 copper = 1 silver at higher values)

    int64_t rounded = std::max<int64_t>(1, std::llround(rawCopper));

    if (rounded >= 1000) rounded = (rounded / 50) * 50;

    else if (rounded >= 50) rounded = (rounded / 5) * 5;

    return static_cast<int32_t>(std::min<int64_t>(rounded, 50000000));

}



int32_t ItemManager::ComputeSuggestedDurability(const DNItem& item) noexcept {

    if (item.rawType != 0 && item.rawType != 1) return 0;

    if (item.group == ItemGroup::Accessory) return 0;

    const int32_t base = (item.rawType == 0) ? 120 : 100;

    const int32_t rankBonus = std::clamp(item.rank, 0, 6) * 20;

    const int32_t lvBonus = (std::clamp(item.levelLimit, 1, 99) / 20) * 10;

    return base + rankBonus + lvBonus;

}



void ItemManager::ApplyStatArchetype(DNItem& item, StatArchetype archetype) {

    for (auto& st : item.states) {

        st.type = -1;

        st.genProb = 0.0f;

        st.minValue = "0";

        st.maxValue = "0";

    }



    auto setSlot = [&](size_t idx, int32_t type) {

        if (idx < kStateSlotCount) {

            item.states[idx].type = type;

            item.states[idx].genProb = 1.0f;

        }

    };



    switch (archetype) {

        case StatArchetype::PhysicalDps:

            setSlot(0, 4);  // Physical Attack Min

            setSlot(1, 5);  // Physical Attack Max

            setSlot(2, 0);  // Strength

            setSlot(3, 1);  // Agility

            setSlot(4, 12); // Critical

            setSlot(5, 29); // Final Damage

            setSlot(6, 54); // Physical Attack Min %

            setSlot(7, 55); // Physical Attack Max %

            break;

        case StatArchetype::MagicalDps:

            setSlot(0, 6);  // Magic Attack Min

            setSlot(1, 7);  // Magic Attack Max

            setSlot(2, 2);  // Intelligence

            setSlot(3, 3);  // Vitality

            setSlot(4, 12); // Critical

            setSlot(5, 29); // Final Damage

            setSlot(6, 56); // Magic Attack Min %

            setSlot(7, 57); // Magic Attack Max %

            break;

        case StatArchetype::TankVitality:

            setSlot(0, 8);  // Physical Defense

            setSlot(1, 9);  // Magic Defense

            setSlot(2, 3);  // Vitality

            setSlot(3, 25); // Max HP

            setSlot(4, 13); // Critical Resistance

            setSlot(5, 53); // Vitality %

            setSlot(6, 75); // Max HP %

            break;

        case StatArchetype::CritFinalDamage:

            setSlot(0, 12); // Critical

            setSlot(1, 29); // Final Damage

            setSlot(2, 0);  // Strength

            setSlot(3, 1);  // Agility

            setSlot(4, 2);  // Intelligence

            setSlot(5, 62); // Critical %

            setSlot(6, 79); // Final Damage %

            break;

        case StatArchetype::ElementalHybrid:

            setSlot(0, 4);  // Physical Attack Min

            setSlot(1, 5);  // Physical Attack Max

            setSlot(2, 6);  // Magic Attack Min

            setSlot(3, 7);  // Magic Attack Max

            setSlot(4, 16); // Fire Attack

            setSlot(5, 17); // Ice Attack

            setSlot(6, 18); // Light Attack

            setSlot(7, 19); // Dark Attack

            setSlot(8, 12); // Critical

            setSlot(9, 25); // Max HP

            break;

        case StatArchetype::BalancedAllStat:

            setSlot(0, 0);  // Strength

            setSlot(1, 1);  // Agility

            setSlot(2, 2);  // Intelligence

            setSlot(3, 3);  // Vitality

            setSlot(4, 4);  // Physical Attack Min

            setSlot(5, 5);  // Physical Attack Max

            setSlot(6, 6);  // Magic Attack Min

            setSlot(7, 7);  // Magic Attack Max

            setSlot(8, 12); // Critical

            setSlot(9, 25); // Max HP

            break;

    }



    AutoScaleStatsByLevelAndRank(item);

}



void ItemManager::AutoScaleStatsByLevelAndRank(DNItem& item) {

    const double lv = static_cast<double>(std::clamp(item.levelLimit, 1, 99));

    const double rankMult = RankPowerMultiplier(item.rank);

    const double slotMult = EquipSlotBudgetWeight(item);

    const double power = (8.0 + lv * 3.8 + lv * lv * 0.28) * rankMult * slotMult;

    const double ratioBase = std::clamp((0.012 + (lv / 95.0) * 0.032) * (0.75 + 0.25 * rankMult) * std::min(1.15, slotMult + 0.25), 0.005, 0.35);



    for (auto& st : item.states) {

        if (st.type < 0) continue;

        if (st.genProb <= 0.0f) st.genProb = 1.0f;



        double baseVal = 0.0;

        double maxMult = 1.0;



        if (st.type >= 0 && st.type <= 3) {

            // STR / AGI / INT / VIT

            baseVal = std::max(2.0, power * 0.34);

        } else if (st.type == 4 || st.type == 6) {

            // Phys / Magic Attack Min

            baseVal = std::max(5.0, power * 1.15);

        } else if (st.type == 5 || st.type == 7) {

            // Phys / Magic Attack Max

            baseVal = std::max(7.0, power * 1.42);

        } else if (st.type == 32 || st.type == 33) {

            baseVal = std::max(6.0, power * 1.25);

        } else if (st.type == 8 || st.type == 9) {

            // Phys / Magic Defense

            baseVal = std::max(6.0, power * 0.92);

        } else if (st.type == 12 || st.type == 13) {

            // Critical / Critical Resistance

            baseVal = std::max(10.0, power * 1.65);

        } else if (st.type == 25) {

            // Max HP

            baseVal = std::max(40.0, power * 14.5);

        } else if (st.type == 26) {

            // Max SP

            baseVal = std::max(25.0, power * 7.0);

        } else if (st.type == 29) {

            // Final Damage

            baseVal = std::max(4.0, power * 0.26);

        } else if (st.type >= 16 && st.type <= 23) {

            // Elemental Attack / Defense ratio

            baseVal = std::clamp(ratioBase * 1.35, 0.01, 0.30);

        } else if (st.type >= 50 && st.type <= 81) {

            // Percentage Stats

            baseVal = ratioBase;

        } else {

            baseVal = std::max(4.0, power * 0.50);

        }



        // Preserve existing min!=max spread habit if the slot already had a random range

        const double oldMin = ParseStatNumber(st.minValue);

        const double oldMax = ParseStatNumber(st.maxValue);

        if (oldMin > 0.0 && oldMax > oldMin * 1.01) {

            maxMult = 1.12;

        }



        st.minValue = FormatStatValue(st.type, baseVal);

        st.maxValue = FormatStatValue(st.type, baseVal * maxMult);

    }



    item.gearScore = ComputeSuggestedGearScore(item);

}



void ItemManager::ScaleItemStats(DNItem& item, double multiplier, bool spreadMax15Pct) {

    if (multiplier <= 0.0) return;

    for (auto& st : item.states) {

        if (st.type < 0) continue;

        const bool hadDot = (st.minValue.find('.') != std::string::npos) ||

                            (st.maxValue.find('.') != std::string::npos) ||

                            IsRatioStateType(st.type);

        const double mn = ParseStatNumber(st.minValue) * multiplier;

        double mx = ParseStatNumber(st.maxValue) * multiplier;

        if (spreadMax15Pct) {

            mx = mn * 1.15;

        } else if (mx < mn) {

            mx = mn;

        }

        st.minValue = FormatStatValue(st.type, mn, hadDot);

        st.maxValue = FormatStatValue(st.type, mx, hadDot);

    }

}



void ItemManager::CompactAndSanitizeStats(DNItem& item) {

    std::array<StateSlot, kStateSlotCount> compacted;

    for (auto& s : compacted) {

        s.type = -1;

        s.genProb = 0.0f;

        s.minValue = "0";

        s.maxValue = "0";

    }



    size_t writeIdx = 0;

    for (const auto& src : item.states) {

        if (src.type < 0) continue;

        StateSlot clean = src;

        clean.genProb = std::clamp(clean.genProb <= 0.0f ? 1.0f : clean.genProb, 0.0f, 1.0f);

        const bool hadDot = (clean.minValue.find('.') != std::string::npos) ||

                            (clean.maxValue.find('.') != std::string::npos) ||

                            IsRatioStateType(clean.type);

        double mn = ParseStatNumber(clean.minValue);

        double mx = ParseStatNumber(clean.maxValue);

        if (mx < mn) std::swap(mn, mx);

        clean.minValue = FormatStatValue(clean.type, mn, hadDot);

        clean.maxValue = FormatStatValue(clean.type, mx, hadDot);

        compacted[writeIdx++] = std::move(clean);

    }

    item.states = std::move(compacted);

}



std::string ItemManager::GenerateRichTooltipDescription(const DNItem& item) const {

    const char* rankColor = "#w";

    switch (item.rank) {

        case 1: rankColor = "#g"; break;

        case 2: rankColor = "#b"; break;

        case 3: rankColor = "#y"; break;

        case 4: rankColor = "#v"; break;

        case 5: rankColor = "#y"; break;

        case 6: rankColor = "#r"; break;

        default: rankColor = "#w"; break;

    }



    const std::string_view slotName = EquipSubTypeName(item);

    const std::string categoryStr = !slotName.empty()

                                        ? std::format("{} ({})", TypeName(item.rawType), slotName)

                                        : TypeName(item.rawType);

    const std::string jobsDesc = DescribeJobs(item.jobClass);



    std::string out = std::format("{}[{} {}]#w\\n", rankColor, RankName(item.rank), categoryStr);

    out += std::format("#yRequired Level:#w Lv.{}", item.levelLimit);

    if (item.heroLevelLimit > 0) {

        out += std::format(" (Hero Lv.{})", item.heroLevelLimit);

    }

    out += std::format("  |  #yClass:#w {}\\n", jobsDesc);



    bool anyStat = false;

    for (const auto& st : item.states) {

        if (st.type < 0) continue;

        if (!anyStat) {

            out += "#gBonus Attributes:#w\\n";

            anyStat = true;

        }

        const char* stName = StateDisplayName(st.type);

        if (IsRatioStateType(st.type)) {

            const double mnPct = ParseStatNumber(st.minValue) * 100.0;

            const double mxPct = ParseStatNumber(st.maxValue) * 100.0;

            if (std::fabs(mxPct - mnPct) > 0.01) {

                out += std::format(" - {}: +{:.2f}% ~ +{:.2f}%\\n", stName, mnPct, mxPct);

            } else {

                out += std::format(" - {}: +{:.2f}%\\n", stName, mnPct);

            }

        } else {

            if (st.minValue != st.maxValue) {

                out += std::format(" - {}: +{} ~ +{}\\n", stName, st.minValue, st.maxValue);

            } else {

                out += std::format(" - {}: +{}\\n", stName, st.minValue);

            }

        }

    }



    if (item.gearScore > 0) {

        out += std::format("#bGear Score:#w {}", item.gearScore);

        if (item.durability > 0) {

            out += std::format("  |  #bDurability:#w {}", item.durability);

        }

    }

    return out;

}



WorkspaceAuditReport ItemManager::RunWorkspaceAudit(size_t maxSamplesPerIssue) const {

    WorkspaceAuditReport rep;

    std::vector<std::string> missingNameSamples;

    std::vector<std::string> missingDetailSamples;

    std::vector<std::string> badStatSamples;

    std::vector<std::string> sharedMidSamples;



    for (const auto& item : m_items) {

        if (item.deleted) continue;

        ++rep.totalChecked;



        if (item.nameId == 0 || Trim(item.name).empty()) {

            ++rep.emptyOrMissingNames;

            if (missingNameSamples.size() < maxSamplesPerIssue) {

                missingNameSamples.push_back(std::format("  - #{} ({}, _NameID={})",

                                                         item.id, GetItemTableName(item.tableIndex), item.nameId));

            }

        } else if (CountItemsUsingMid(item.nameId) > 1) {

            ++rep.sharedNameMids;

            if (sharedMidSamples.size() < maxSamplesPerIssue) {

                sharedMidSamples.push_back(std::format("  - #{} '{}' shares _NameID {} with {} items",

                                                       item.id, item.name, item.nameId, CountItemsUsingMid(item.nameId)));

            }

        }



        if ((item.rawType == 0 && !m_weaponTableById.contains(item.id)) ||

            (item.rawType == 1 && !m_partsTableById.contains(item.id))) {

            ++rep.missingEquipDetailRows;

            if (missingDetailSamples.size() < maxSamplesPerIssue) {

                missingDetailSamples.push_back(std::format("  - #{} '{}' ({}, Type={}) missing {} row",

                                                           item.id, item.name, GetItemTableName(item.tableIndex),

                                                           item.rawType, item.rawType == 0 ? "weapontable" : "partstable"));

            }

        }



        bool hasBadStat = false;

        std::string badStatReason;

        for (size_t s = 0; s < kStateSlotCount; ++s) {

            const auto& st = item.states[s];

            if (st.type < 0) continue;

            const double mn = ParseStatNumber(st.minValue);

            const double mx = ParseStatNumber(st.maxValue);

            if (mx + 1e-6 < mn) {

                hasBadStat = true;

                badStatReason = std::format("Slot {} ({}) min {} > max {}", s + 1, StateDisplayName(st.type), st.minValue, st.maxValue);

                break;

            }

            if (st.genProb <= 0.0f && (mn != 0.0 || mx != 0.0)) {

                hasBadStat = true;

                badStatReason = std::format("Slot {} ({}) active value with GenProb=0", s + 1, StateDisplayName(st.type));

                break;

            }

        }

        if (hasBadStat) {

            ++rep.invalidStatRanges;

            if (badStatSamples.size() < maxSamplesPerIssue) {

                badStatSamples.push_back(std::format("  - #{} '{}' ({}) -> {}",

                                                     item.id, item.name, GetItemTableName(item.tableIndex), badStatReason));

            }

        }

    }



    std::string txt;

    txt.reserve(8192);

    txt += std::format("=== Dragon Nest Workspace Integrity Audit ===\r\n"

                       "Resource Folder         : {}\r\n"

                       "Active Items Scanned    : {}\r\n"

                       "Session Modified Items  : {}\r\n"

                       "Empty / Missing Names   : {}\r\n"

                       "Missing Weapon/Parts Row: {}\r\n"

                       "Invalid Stat Slots      : {}\r\n"

                       "Shared _NameID Collisions: {} item(s)\r\n\r\n",

                       m_resourcePath.string(),

                       rep.totalChecked,

                       m_modifiedItemIds.size(),

                       rep.emptyOrMissingNames,

                       rep.missingEquipDetailRows,

                       rep.invalidStatRanges,

                       rep.sharedNameMids);



    auto appendSection = [&](const char* title, size_t total, const std::vector<std::string>& samples) {

        txt += std::format("[{}] ({} total)\r\n", title, total);

        if (samples.empty()) {

            txt += "  (None detected - clean!)\r\n\r\n";

            return;

        }

        for (const auto& line : samples) {

            txt += line + "\r\n";

        }

        if (total > samples.size()) {

            txt += std::format("  ... and {} more.\r\n", total - samples.size());

        }

        txt += "\r\n";

    };



    appendSection("Equipment Missing Weapon/Parts Detail Row", rep.missingEquipDetailRows, missingDetailSamples);

    appendSection("Invalid Stat Ranges / Zero GenProb Slots", rep.invalidStatRanges, badStatSamples);

    appendSection("Empty or Missing UIString Names", rep.emptyOrMissingNames, missingNameSamples);

    appendSection("Shared _NameID Collision Warning (Use 'Fork Exclusive MIDs' before renaming)", rep.sharedNameMids, sharedMidSamples);



    rep.reportText = std::move(txt);

    return rep;

}



namespace {



void AppendJsonEscaped(std::string& out, std::string_view sv) {

    out.push_back('"');

    for (unsigned char c : sv) {

        switch (c) {

            case '"':  out += "\\\""; break;

            case '\\': out += "\\\\"; break;

            case '\b': out += "\\b";  break;

            case '\f': out += "\\f";  break;

            case '\n': out += "\\n";  break;

            case '\r': out += "\\r";  break;

            case '\t': out += "\\t";  break;

            default:

                if (c < 0x20u) {

                    out += std::format("\\u{:04x}", static_cast<unsigned>(c));

                } else {

                    out.push_back(static_cast<char>(c));

                }

                break;

        }

    }

    out.push_back('"');

}



struct JsonValue {

    enum class Kind : uint8_t { Null, Bool, Number, String, Array, Object };

    Kind kind = Kind::Null;

    bool bVal = false;

    double nVal = 0.0;

    std::string sVal;

    std::vector<JsonValue> arr;

    std::vector<std::pair<std::string, JsonValue>> obj;



    const JsonValue* Find(std::string_view key) const noexcept {

        if (kind != Kind::Object) return nullptr;

        for (const auto& kv : obj) {

            if (kv.first == key) return &kv.second;

        }

        return nullptr;

    }

    int32_t GetInt(std::string_view key, int32_t def = 0) const noexcept {

        if (const auto* v = Find(key)) {

            if (v->kind == Kind::Number) return static_cast<int32_t>(std::llround(v->nVal));

        }

        return def;

    }

    float GetFloat(std::string_view key, float def = 0.0f) const noexcept {

        if (const auto* v = Find(key)) {

            if (v->kind == Kind::Number) return static_cast<float>(v->nVal);

        }

        return def;

    }

    bool GetBool(std::string_view key, bool def = false) const noexcept {

        if (const auto* v = Find(key)) {

            if (v->kind == Kind::Bool) return v->bVal;

            if (v->kind == Kind::Number) return v->nVal != 0.0;

        }

        return def;

    }

    std::string GetString(std::string_view key, std::string_view def = {}) const {

        if (const auto* v = Find(key)) {

            if (v->kind == Kind::String) return v->sVal;

            if (v->kind == Kind::Number) return std::to_string(static_cast<int64_t>(std::llround(v->nVal)));

        }

        return std::string(def);

    }

};



class MiniJsonParser {

public:

    explicit MiniJsonParser(std::string_view src) : m_src(src) {}



    bool ParseRoot(JsonValue& out, std::string& err) {

        SkipWs();

        if (!ParseValue(out, err, 0)) return false;

        SkipWs();

        return true;

    }



private:

    void SkipWs() noexcept {

        // Also skip UTF-8 BOM at offset 0 if present

        if (m_pos == 0 && m_src.size() >= 3 &&

            static_cast<uint8_t>(m_src[0]) == 0xEF &&

            static_cast<uint8_t>(m_src[1]) == 0xBB &&

            static_cast<uint8_t>(m_src[2]) == 0xBF) {

            m_pos = 3;

        }

        while (m_pos < m_src.size()) {

            const char c = m_src[m_pos];

            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++m_pos;

            else break;

        }

    }



    bool ParseValue(JsonValue& out, std::string& err, int depth) {

        if (depth > 32) {

            err = "JSON nesting too deep";

            return false;

        }

        SkipWs();

        if (m_pos >= m_src.size()) {

            err = "Unexpected end of JSON";

            return false;

        }

        const char c = m_src[m_pos];

        if (c == '{') return ParseObject(out, err, depth + 1);

        if (c == '[') return ParseArray(out, err, depth + 1);

        if (c == '"') {

            out.kind = JsonValue::Kind::String;

            return ParseString(out.sVal, err);

        }

        if (c == 't' && m_src.substr(m_pos, 4) == "true") {

            m_pos += 4;

            out.kind = JsonValue::Kind::Bool;

            out.bVal = true;

            return true;

        }

        if (c == 'f' && m_src.substr(m_pos, 5) == "false") {

            m_pos += 5;

            out.kind = JsonValue::Kind::Bool;

            out.bVal = false;

            return true;

        }

        if (c == 'n' && m_src.substr(m_pos, 4) == "null") {

            m_pos += 4;

            out.kind = JsonValue::Kind::Null;

            return true;

        }

        if (c == '-' || (c >= '0' && c <= '9')) {

            return ParseNumber(out, err);

        }

        err = std::format("Unexpected token '{}' at offset {}", c, m_pos);

        return false;

    }



    bool ParseString(std::string& out, std::string& err) {

        if (m_pos >= m_src.size() || m_src[m_pos] != '"') return false;

        ++m_pos;

        out.clear();

        while (m_pos < m_src.size()) {

            const char c = m_src[m_pos++];

            if (c == '"') return true;

            if (c == '\\') {

                if (m_pos >= m_src.size()) break;

                const char esc = m_src[m_pos++];

                switch (esc) {

                    case '"':  out.push_back('"'); break;

                    case '\\': out.push_back('\\'); break;

                    case '/':  out.push_back('/'); break;

                    case 'b':  out.push_back('\b'); break;

                    case 'f':  out.push_back('\f'); break;

                    case 'n':  out.push_back('\n'); break;

                    case 'r':  out.push_back('\r'); break;

                    case 't':  out.push_back('\t'); break;

                    case 'u': {

                        if (m_pos + 4 > m_src.size()) {

                            err = "Incomplete \\uXXXX escape";

                            return false;

                        }

                        uint32_t cp = 0;

                        const auto [ptr, ec] = std::from_chars(m_src.data() + m_pos, m_src.data() + m_pos + 4, cp, 16);

                        if (ec != std::errc{} || ptr != m_src.data() + m_pos + 4) {

                            err = "Invalid \\uXXXX hex digits";

                            return false;

                        }

                        m_pos += 4;

                        if (cp < 0x80u) {

                            out.push_back(static_cast<char>(cp));

                        } else if (cp < 0x800u) {

                            out.push_back(static_cast<char>(0xC0u | (cp >> 6)));

                            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));

                        } else {

                            out.push_back(static_cast<char>(0xE0u | (cp >> 12)));

                            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));

                            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));

                        }

                        break;

                    }

                    default: out.push_back(esc); break;

                }

            } else {

                out.push_back(c);

            }

        }

        err = "Unterminated JSON string";

        return false;

    }



    bool ParseNumber(JsonValue& out, std::string& err) {

        const char* start = m_src.data() + m_pos;

        const char* end = m_src.data() + m_src.size();

        double val = 0.0;

        const auto [ptr, ec] = std::from_chars(start, end, val);

        if (ec != std::errc{} || ptr == start) {

            err = std::format("Invalid number at offset {}", m_pos);

            return false;

        }

        m_pos = static_cast<size_t>(ptr - m_src.data());

        out.kind = JsonValue::Kind::Number;

        out.nVal = val;

        return true;

    }



    bool ParseArray(JsonValue& out, std::string& err, int depth) {

        ++m_pos; // '['

        out.kind = JsonValue::Kind::Array;

        out.arr.clear();

        SkipWs();

        if (m_pos < m_src.size() && m_src[m_pos] == ']') {

            ++m_pos;

            return true;

        }

        while (m_pos < m_src.size()) {

            JsonValue elem;

            if (!ParseValue(elem, err, depth)) return false;

            out.arr.push_back(std::move(elem));

            SkipWs();

            if (m_pos < m_src.size() && m_src[m_pos] == ',') {

                ++m_pos;

                continue;

            }

            if (m_pos < m_src.size() && m_src[m_pos] == ']') {

                ++m_pos;

                return true;

            }

            break;

        }

        err = std::format("Expected ',' or ']' in array at offset {}", m_pos);

        return false;

    }



    bool ParseObject(JsonValue& out, std::string& err, int depth) {

        ++m_pos; // '{'

        out.kind = JsonValue::Kind::Object;

        out.obj.clear();

        SkipWs();

        if (m_pos < m_src.size() && m_src[m_pos] == '}') {

            ++m_pos;

            return true;

        }

        while (m_pos < m_src.size()) {

            SkipWs();

            std::string key;

            if (!ParseString(key, err)) return false;

            SkipWs();

            if (m_pos >= m_src.size() || m_src[m_pos] != ':') {

                err = std::format("Expected ':' after object key at offset {}", m_pos);

                return false;

            }

            ++m_pos;

            JsonValue val;

            if (!ParseValue(val, err, depth)) return false;

            out.obj.emplace_back(std::move(key), std::move(val));

            SkipWs();

            if (m_pos < m_src.size() && m_src[m_pos] == ',') {

                ++m_pos;

                continue;

            }

            if (m_pos < m_src.size() && m_src[m_pos] == '}') {

                ++m_pos;

                return true;

            }

            break;

        }

        err = std::format("Expected ',' or '}}' in object at offset {}", m_pos);

        return false;

    }



    std::string_view m_src;

    size_t m_pos = 0;

};



} // namespace



std::string ItemManager::ExportItemsToJson(const std::vector<int32_t>& itemIds) const {

    std::string out;

    out.reserve(itemIds.size() * 640 + 128);

    out += "{\r\n  \"format\": \"DNItemMaker.ItemPack.v1\",\r\n  \"items\": [\r\n";



    bool firstItem = true;

    for (int32_t id : itemIds) {

        const DNItem* it = GetItem(id);

        if (!it) continue;

        if (!firstItem) out += ",\r\n";

        firstItem = false;



        out += "    {\r\n";

        out += std::format("      \"id\": {},\r\n", it->id);

        out += "      \"sourceTable\": "; AppendJsonEscaped(out, GetItemTableName(it->tableIndex)); out += ",\r\n";

        out += "      \"name\": "; AppendJsonEscaped(out, it->name); out += ",\r\n";

        out += "      \"description\": "; AppendJsonEscaped(out, it->description); out += ",\r\n";

        out += std::format("      \"rawType\": {},\r\n", it->rawType);

        out += std::format("      \"equipSubType\": {},\r\n", it->equipSubType);

        out += std::format("      \"rank\": {},\r\n", it->rank);

        out += std::format("      \"levelLimit\": {},\r\n", it->levelLimit);

        out += std::format("      \"heroLevelLimit\": {},\r\n", it->heroLevelLimit);

        out += "      \"jobClass\": "; AppendJsonEscaped(out, it->jobClass); out += ",\r\n";

        out += std::format("      \"overlapCount\": {},\r\n", it->overlapCount);

        out += std::format("      \"sellAmount\": {},\r\n", it->sellAmount);

        out += std::format("      \"iconIndex\": {},\r\n", it->iconIndex);

        out += std::format("      \"gearScore\": {},\r\n", it->gearScore);

        out += std::format("      \"durability\": {},\r\n", it->durability);

        out += std::format("      \"isCash\": {},\r\n", it->isCash ? "true" : "false");

        out += std::format("      \"ableStorage\": {},\r\n", it->ableStorage ? "true" : "false");

        out += std::format("      \"ableWStorage\": {},\r\n", it->ableWStorage ? "true" : "false");

        out += "      \"states\": [\r\n";



        bool firstState = true;

        for (size_t s = 0; s < kStateSlotCount; ++s) {

            const auto& st = it->states[s];

            if (st.type < 0) continue;

            if (!firstState) out += ",\r\n";

            firstState = false;

            out += std::format("        {{ \"slot\": {}, \"type\": {}, \"prob\": {:.4f}, \"min\": ",

                               s, st.type, st.genProb);

            AppendJsonEscaped(out, st.minValue);

            out += ", \"max\": ";

            AppendJsonEscaped(out, st.maxValue);

            out += " }";

        }

        if (!firstState) out += "\r\n";

        out += "      ]\r\n";

        out += "    }";

    }



    if (!firstItem) out += "\r\n";

    out += "  ]\r\n}\r\n";

    return out;

}



size_t ItemManager::ImportItemsFromJson(std::string_view jsonText, int targetTableIndex,

                                        std::vector<int32_t>& outCreatedIds, std::string& outReport) {

    outCreatedIds.clear();

    if (m_itemTables.empty()) {

        outReport = "No itemtable*.dnt loaded.";

        return 0;

    }



    JsonValue root;

    std::string parseErr;

    MiniJsonParser parser(jsonText);

    if (!parser.ParseRoot(root, parseErr)) {

        outReport = std::format("JSON parse error: {}", parseErr);

        return 0;

    }



    const std::vector<JsonValue>* itemsArr = nullptr;

    if (root.kind == JsonValue::Kind::Array) {

        itemsArr = &root.arr;

    } else if (root.kind == JsonValue::Kind::Object) {

        if (const auto* arrNode = root.Find("items"); arrNode && arrNode->kind == JsonValue::Kind::Array) {

            itemsArr = &arrNode->arr;

        }

    }



    if (!itemsArr) {

        outReport = "JSON root must be an object containing an \"items\" array or a direct array of item objects.";

        return 0;

    }



    for (const auto& node : *itemsArr) {

        if (node.kind != JsonValue::Kind::Object) continue;



        int dstTbl = targetTableIndex;

        if (dstTbl < 0) {

            const std::string srcTblName = node.GetString("sourceTable");

            if (!srcTblName.empty()) {

                dstTbl = FindItemTableByName(srcTblName);

            }

            if (dstTbl < 0) dstTbl = 0;

        }



        const int32_t reqId = node.GetInt("id", 0);

        const int32_t rawType = node.GetInt("rawType", 2);

        const int32_t equipSub = node.GetInt("equipSubType", rawType == 0 ? 0 : (rawType == 1 ? 3 : -1));

        const std::string name = node.GetString("name", "Imported Item");



        const int32_t useId = (reqId > 0 && !IsIdUsedAnywhere(reqId)) ? reqId : 0;

        const int32_t createdId = CreateNewItem(dstTbl, rawType, equipSub, name, useId);

        if (createdId <= 0) continue;



        const DNItem* basePtr = GetItem(createdId);

        if (!basePtr) continue;

        DNItem edited = *basePtr;



        edited.nameTemplate = name;

        edited.descTemplate = node.GetString("description");

        edited.rank = std::clamp(node.GetInt("rank", 0), 0, 6);

        edited.levelLimit = std::clamp(node.GetInt("levelLimit", 1), 1, 999);

        edited.heroLevelLimit = std::max(0, node.GetInt("heroLevelLimit", 0));

        edited.jobClass = node.GetString("jobClass", "0");

        edited.overlapCount = std::max(1, node.GetInt("overlapCount", 1));

        edited.sellAmount = std::max(0, node.GetInt("sellAmount", 0));

        edited.iconIndex = std::max(0, node.GetInt("iconIndex", 0));

        edited.gearScore = std::max(0, node.GetInt("gearScore", 0));

        edited.durability = std::max(0, node.GetInt("durability", edited.durability));

        edited.isCash = node.GetBool("isCash", false);

        edited.ableStorage = node.GetBool("ableStorage", true);

        edited.ableWStorage = node.GetBool("ableWStorage", true);



        if (const auto* statesNode = node.Find("states"); statesNode && statesNode->kind == JsonValue::Kind::Array) {

            size_t autoSlot = 0;

            for (const auto& sNode : statesNode->arr) {

                if (sNode.kind != JsonValue::Kind::Object) continue;

                const int explicitSlot = sNode.GetInt("slot", -1);

                const size_t slotIdx = (explicitSlot >= 0 && static_cast<size_t>(explicitSlot) < kStateSlotCount)

                                           ? static_cast<size_t>(explicitSlot)

                                           : autoSlot;

                if (slotIdx >= kStateSlotCount) continue;

                edited.states[slotIdx].type = sNode.GetInt("type", -1);

                edited.states[slotIdx].genProb = std::clamp(sNode.GetFloat("prob", 1.0f), 0.0f, 1.0f);

                edited.states[slotIdx].minValue = sNode.GetString("min", "0");

                edited.states[slotIdx].maxValue = sNode.GetString("max", edited.states[slotIdx].minValue);

                autoSlot = std::min(kStateSlotCount, slotIdx + 1);

            }

        }



        // Optional visual source item cloning if JSON specifies "visualSourceId"

        const int32_t visualSrcId = node.GetInt("visualSourceId", 0);

        ApplyItem(edited);

        if (visualSrcId > 0 && GetItem(visualSrcId) != nullptr) {

            CopyVisualFromItem(createdId, visualSrcId, node.Find("iconIndex") == nullptr, true);

        }

        outCreatedIds.push_back(createdId);

    }



    outReport = std::format("Imported {} item(s) from JSON pack.", outCreatedIds.size());

    return outCreatedIds.size();

}





bool ItemManager::HasStats(const DNItem& item) noexcept {

    for (size_t i = 0; i < kStateSlotCount; ++i) {

        if (item.states[i].type >= 0) return true;

    }

    return item.rawType == 0 || item.rawType == 1 ||

           item.group == ItemGroup::Weapon ||

           item.group == ItemGroup::Armor ||

           item.group == ItemGroup::Accessory;

}



bool ItemManager::HasModel(const DNItem& item) const {

    if (item.rawType != 0 && item.rawType != 1) return false;

    return !FindSkinPath(item).empty();

}



int32_t ItemManager::GetItemCharmNum(const DNItem& item) const noexcept {

    if (item.typeParam1 > 0 && m_charmGroups.contains(item.typeParam1)) {

        return item.typeParam1;

    }

    const auto itKey = m_charmItemToCharmNum.find(item.id);

    if (itKey != m_charmItemToCharmNum.end()) {

        return itKey->second;

    }

    if (item.typeParam2 > 0 && m_charmGroups.contains(item.typeParam2)) {

        return item.typeParam2;

    }

    if (item.typeParam1 > 0 && (item.rawType == 46 || item.rawType == 112 || item.rawType == 122 || item.rawType == 8)) {

        return item.typeParam1;

    }

    return 0;

}



bool ItemManager::HasCharm(const DNItem& item) const noexcept {

    if (GetItemCharmNum(item) > 0) return true;

    if (item.rawType == 46 || item.rawType == 112 || item.rawType == 122 || item.rawType == 8) return true;

    return m_charmKeys.contains(item.id);

}



const CharmGroup* ItemManager::GetCharmGroup(int32_t charmNum) const {

    const auto it = m_charmGroups.find(charmNum);

    return (it != m_charmGroups.end()) ? &it->second : nullptr;

}



const CharmKeyInfo* ItemManager::GetCharmKeyInfo(int32_t charmItemId) const {

    const auto it = m_charmKeys.find(charmItemId);

    return (it != m_charmKeys.end()) ? &it->second : nullptr;

}



bool ItemManager::SetCharmItemProb(int32_t rowId, int32_t newWeight) {

    const auto lIt = m_charmRowLookup.find(rowId);

    if (lIt == m_charmRowLookup.end()) return false;

    const size_t pairIdx = lIt->second.first;

    const int32_t cNum = lIt->second.second;

    if (pairIdx >= m_charmTables.size()) return false;

    auto& pair = m_charmTables[pairIdx];



    if (DNTRow* pr = pair.probTable.GetRow(rowId)) {

        if (pair.cProbInProb >= 0) pair.probTable.SetInt(*pr, static_cast<size_t>(pair.cProbInProb), newWeight);

        pair.dirtyProb = true;

    } else if (DNTRow* ir = pair.itemTable.GetRow(rowId)) {

        if (pair.cProbInItem >= 0) pair.itemTable.SetInt(*ir, static_cast<size_t>(pair.cProbInItem), newWeight);

        pair.dirtyItem = true;

    }



    const auto gIt = m_charmGroups.find(cNum);

    if (gIt != m_charmGroups.end()) {

        auto& group = gIt->second;

        int64_t totalW = 0;

        for (auto& rew : group.rewards) {

            if (rew.rowId == rowId) rew.probWeight = newWeight;

            if (rew.probWeight > 0) totalW += rew.probWeight;

        }

        group.totalWeight = totalW;

        double cum = 0.0;

        for (auto& rew : group.rewards) {

            if (totalW > 0 && rew.probWeight > 0) {

                rew.dropRatePercent = (static_cast<double>(rew.probWeight) / static_cast<double>(totalW)) * 100.0;

                cum += rew.dropRatePercent;

                rew.cumulativePercent = std::min(100.0, cum);

            } else {

                rew.dropRatePercent = 0.0;

                rew.cumulativePercent = cum;

            }

        }

    }

    return true;

}



bool ItemManager::NormalizeCharmGroupWeights(int32_t charmNum, int64_t targetTotalWeight) {

    if (targetTotalWeight <= 0) targetTotalWeight = 100000;

    const auto gIt = m_charmGroups.find(charmNum);

    if (gIt == m_charmGroups.end() || gIt->second.totalWeight <= 0) return false;

    auto& group = gIt->second;

    const double factor = static_cast<double>(targetTotalWeight) / static_cast<double>(group.totalWeight);

    int64_t runningSum = 0;

    for (size_t i = 0; i < group.rewards.size(); ++i) {

        auto& rew = group.rewards[i];

        int32_t scaled = 0;

        if (i + 1 == group.rewards.size()) {

            scaled = static_cast<int32_t>(targetTotalWeight - runningSum);

        } else {

            scaled = static_cast<int32_t>(std::round(rew.probWeight * factor));

            runningSum += scaled;

        }

        if (scaled < 0) scaled = 0;

        SetCharmItemProb(rew.rowId, scaled);

    }

    return true;

}




bool ItemManager::IsDragonJewel(const DNItem& item) const noexcept {
    return m_dragonJewels.contains(item.id) || item.rawType == 43;
}

bool ItemManager::HasJewelSockets(const DNItem& item) const noexcept {
    return item.dragonJewelType > 0 && m_jewelSlotConfigs.contains(item.dragonJewelType);
}

bool ItemManager::IsSetItem(const DNItem& item) const noexcept {
    return item.setItemId > 0 && m_setItems.contains(item.setItemId);
}

bool ItemManager::IsEnhanceable(const DNItem& item) const noexcept {
    return item.enchantId > 0 && m_enchantIndex.contains(item.enchantId);
}

const DragonJewelInfo* ItemManager::GetDragonJewelInfo(int32_t itemId) const {
    const auto it = m_dragonJewels.find(itemId);
    return it != m_dragonJewels.end() ? &it->second : nullptr;
}

const DragonJewelSlotConfig* ItemManager::GetItemJewelSlots(int32_t dragonJewelType) const {
    const auto it = m_jewelSlotConfigs.find(dragonJewelType);
    return it != m_jewelSlotConfigs.end() ? &it->second : nullptr;
}

const SetItemGroup* ItemManager::GetSetItemGroup(int32_t setItemId) const {
    const auto it = m_setItems.find(setItemId);
    return it != m_setItems.end() ? &it->second : nullptr;
}

std::vector<DragonJewelInfo> ItemManager::GetCompatibleJewels(int32_t slotType, int32_t levelLimit, int32_t rank) const {
    std::vector<DragonJewelInfo> results;
    results.reserve(64);
    for (const auto& [id, info] : m_dragonJewels) {
        if (slotType > 0 && info.type != slotType) continue;
        const DNItem* item = GetItem(id);
        if (item) {
            if (item->levelLimit > levelLimit) continue;
            if (rank >= 0 && item->rank != rank) continue;
        }
        results.push_back(info);
    }
    std::sort(results.begin(), results.end(), [this](const DragonJewelInfo& a, const DragonJewelInfo& b) {
        const DNItem* ia = GetItem(a.itemId);
        const DNItem* ib = GetItem(b.itemId);
        const int la = ia ? ia->levelLimit : 0;
        const int lb = ib ? ib->levelLimit : 0;
        if (la != lb) return la > lb;
        return a.itemId < b.itemId;
    });
    return results;
}

std::vector<int32_t> ItemManager::GetCompatibleEquipmentForJewel(const DragonJewelInfo& jewel) const {
    std::vector<int32_t> results;
    results.reserve(128);
    for (const auto& item : m_items) {
        if (item.deleted || item.dragonJewelType <= 0) continue;
        const auto* cfg = GetItemJewelSlots(item.dragonJewelType);
        if (!cfg) continue;

        bool acceptsType = false;
        for (int s : cfg->slots) {
            if (s == jewel.type) {
                acceptsType = true;
                break;
            }
        }
        if (!acceptsType) continue;

        if (jewel.applicableSlots != 0) {
            int slotBit = -1;
            if (item.rawType == 0) {
                slotBit = (item.equipSubType == 0) ? 0 : 1;
            } else if (item.rawType == 1) {
                switch (item.equipSubType) {
                    case 0: slotBit = 2; break;
                    case 1: slotBit = 3; break;
                    case 2: slotBit = 4; break;
                    case 3: slotBit = 5; break;
                    case 4: slotBit = 6; break;
                    case 5: slotBit = 7; break;
                    case 6: slotBit = 8; break;
                    case 7: slotBit = 9; break;
                    default: break;
                }
            }
            if (slotBit >= 0 && ((jewel.applicableSlots & (1 << slotBit)) == 0)) {
                continue;
            }
        }
        results.push_back(item.id);
    }
    return results;
}

EnchantCurve ItemManager::GetEnchantCurve(int32_t enchantId) const {
    EnchantCurve curve;
    curve.enchantId = enchantId;
    const auto it = m_enchantIndex.find(enchantId);
    if (it == m_enchantIndex.end()) return curve;

    curve.steps.reserve(it->second.size());
    for (const auto& [tIdx, rIdx] : it->second) {
        if (tIdx >= m_enchantTables.size()) continue;
        const auto& et = m_enchantTables[tIdx];
        if (rIdx >= et.GetRowCount()) continue;
        const auto& r = et.GetRows()[rIdx];

        EnchantStep step;
        step.enchantId = enchantId;

        const int cLvl = et.GetColumnIndex("_EnchantLevel");
        const int cRatio = et.GetColumnIndex("_EnchantRatio");
        const int cBreak = et.GetColumnIndex("_BreakRatio");
        const int cMinDown = et.GetColumnIndex("_MinDown");
        const int cMaxDown = et.GetColumnIndex("_MaxDown");
        const int cCoin = et.GetColumnIndex("_NeedCoin");
        const int cCoin64 = et.GetColumnIndex("_NeedCoin64");
        const int cProtect = et.GetColumnIndex("_ProtectItemCount");
        const int cGear = et.GetColumnIndex("_GearScore");

        step.level = cLvl >= 0 ? et.GetInt(r, static_cast<size_t>(cLvl)) : 0;
        step.successRatio = cRatio >= 0 ? et.GetFloat(r, static_cast<size_t>(cRatio)) : 0.0f;
        step.breakRatio = cBreak >= 0 ? et.GetFloat(r, static_cast<size_t>(cBreak)) : 0.0f;
        step.minDown = cMinDown >= 0 ? et.GetInt(r, static_cast<size_t>(cMinDown)) : 0;
        step.maxDown = cMaxDown >= 0 ? et.GetInt(r, static_cast<size_t>(cMaxDown)) : 0;

        int64_t coin = 0;
        if (cCoin64 >= 0) coin = et.GetInt64(r, static_cast<size_t>(cCoin64));
        if (coin == 0 && cCoin >= 0) coin = et.GetInt(r, static_cast<size_t>(cCoin));
        step.needCoin = coin;

        if (cProtect >= 0) step.protectItemCount = et.GetInt(r, static_cast<size_t>(cProtect));
        if (cGear >= 0) step.gearScore = et.GetInt(r, static_cast<size_t>(cGear));

        for (int i = 1; i <= 5; ++i) {
            const int cItem = et.GetColumnIndex(std::format("_NeedItemID{}", i));
            const int cCount = et.GetColumnIndex(std::format("_NeedItemCount{}", i));
            step.needItemIds[static_cast<size_t>(i - 1)] = cItem >= 0 ? et.GetInt(r, static_cast<size_t>(cItem)) : 0;
            step.needItemCounts[static_cast<size_t>(i - 1)] = cCount >= 0 ? et.GetInt(r, static_cast<size_t>(cCount)) : 0;
        }

        for (int i = 1; i <= 10; ++i) {
            const int cSt = et.GetColumnIndex(std::format("_State{}", i));
            const int cVal = et.GetColumnIndex(std::format("_State{}Value", i));
            if (cSt >= 0 && cVal >= 0) {
                const int32_t stType = et.GetInt(r, static_cast<size_t>(cSt));
                const std::string& stVal = et.GetString(r, static_cast<size_t>(cVal));
                if (stType >= 0 && !stVal.empty() && stVal != "0") {
                    step.stats.push_back({ stType, stVal });
                }
            }
        }

        curve.steps.push_back(std::move(step));
    }

    std::sort(curve.steps.begin(), curve.steps.end(), [](const EnchantStep& a, const EnchantStep& b) {
        return a.level < b.level;
    });

    return curve;
}

PotentialGroup ItemManager::GetPotentialGroup(int32_t potentialId) const {
    PotentialGroup group;
    group.potentialId = potentialId;
    const auto it = m_potentialIndex.find(potentialId);
    if (it == m_potentialIndex.end()) return group;

    group.variants.reserve(it->second.size());
    for (const auto& [tIdx, rIdx] : it->second) {
        if (tIdx >= m_potentialTables.size()) continue;
        const auto& pt = m_potentialTables[tIdx];
        if (rIdx >= pt.GetRowCount()) continue;
        const auto& r = pt.GetRows()[rIdx];

        PotentialVariant v;
        v.potentialId = potentialId;
        const int cNo = pt.GetColumnIndex("_PotentialNo");
        const int cRatio = pt.GetColumnIndex("_PotentialRatio");
        const int cPrefix = pt.GetColumnIndex("_PrefixNameID");

        v.potentialNo = cNo >= 0 ? pt.GetInt(r, static_cast<size_t>(cNo)) : 0;
        v.ratio = cRatio >= 0 ? pt.GetFloat(r, static_cast<size_t>(cRatio)) : 0.0f;
        v.prefixMid = cPrefix >= 0 ? static_cast<uint32_t>(pt.GetInt(r, static_cast<size_t>(cPrefix))) : 0;
        if (v.prefixMid > 0) v.prefixName = m_uiStrings.GetString(v.prefixMid);

        for (int i = 1; i <= 16; ++i) {
            const int cSt = pt.GetColumnIndex(std::format("_State{}", i));
            const int cVal = pt.GetColumnIndex(std::format("_State{}Value", i));
            if (cSt >= 0 && cVal >= 0) {
                const int32_t stType = pt.GetInt(r, static_cast<size_t>(cSt));
                const std::string& stVal = pt.GetString(r, static_cast<size_t>(cVal));
                if (stType >= 0 && !stVal.empty() && stVal != "0") {
                    v.stats.push_back({ stType, stVal });
                }
            }
        }
        group.variants.push_back(std::move(v));
    }

    std::sort(group.variants.begin(), group.variants.end(), [](const PotentialVariant& a, const PotentialVariant& b) {
        return a.potentialNo < b.potentialNo;
    });

    return group;
}

bool ItemManager::SetItemDragonJewelType(int32_t itemId, int32_t newType) {
    auto it = m_indexById.find(itemId);
    if (it == m_indexById.end()) return false;
    DNItem& item = m_items[it->second];
    item.dragonJewelType = newType;
    WriteItemRow(item);
    m_dirtyItems[item.tableIndex] = true;
    m_modifiedItemIds.insert(itemId);
    return true;
}

bool ItemManager::SetItemEnchantId(int32_t itemId, int32_t newEnchantId) {
    auto it = m_indexById.find(itemId);
    if (it == m_indexById.end()) return false;
    DNItem& item = m_items[it->second];
    item.enchantId = newEnchantId;
    WriteItemRow(item);
    m_dirtyItems[item.tableIndex] = true;
    m_modifiedItemIds.insert(itemId);
    return true;
}

bool ItemManager::SetDragonJewelAttributes(int32_t itemId, int32_t type, int32_t elementalType, int32_t applicableSlots) {
    if (DNTRow* r = m_dragonJewelTable.GetRow(itemId)) {
        const int cType = m_dragonJewelTable.GetColumnIndex("_Type");
        const int cElem = m_dragonJewelTable.GetColumnIndex("_ElementalType");
        const int cApp = m_dragonJewelTable.GetColumnIndex("_ApplicableValue");
        if (cType >= 0) m_dragonJewelTable.SetInt(*r, static_cast<size_t>(cType), type);
        if (cElem >= 0) m_dragonJewelTable.SetInt(*r, static_cast<size_t>(cElem), elementalType);
        if (cApp >= 0) m_dragonJewelTable.SetInt(*r, static_cast<size_t>(cApp), applicableSlots);
        m_dirtyDragonJewelTable = true;
        m_modifiedItemIds.insert(itemId);
        auto djIt = m_dragonJewels.find(itemId);
        if (djIt != m_dragonJewels.end()) {
            djIt->second.type = type;
            djIt->second.elementalType = elementalType;
            djIt->second.applicableSlots = applicableSlots;
        }
        return true;
    }
    return false;
}

} // namespace DNItemMaker::Data


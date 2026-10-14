// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the players' own colours for the characters. In the folder "palettes" beside settings.ini,
// a copy of the set's vm3j.10b changed with PalMod gives its colours for the characters (their sets and their
// effects' palettes), and a text file for a character new colours for any of its sets (vsav.h, kColourSets),
// over the PalMod file's. They are written over the game's in the ROM image, where the game reads them each time it colours a player, so the
// game's code is the same with them as without. A command writes the game's own colours out as files to
// start from.
#include "vsav.h"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace cps2recomp {

namespace {
    using namespace vsav;

    constexpr const char *kFeature = "custom_colours";
    constexpr const char *kFolder = "palettes", *kGameColours = "original";

    // The characters that have colours: their numbers, the names this set of the game gives them and the
    // names the game has abroad (files may be called by either)
    struct Character {
        int number;
        const char *name;
        const char *abroad;
    };
    constexpr Character kList[] = {
        {0, "Bulleta", "BB Hood"},     {1, "Demitri", nullptr},        {2, "Gallon", "Talbain"},
        {3, "Victor", nullptr},        {4, "Zabel", "Lord Raptor"},    {5, "Morrigan", nullptr},
        {6, "Anakaris", nullptr},      {7, "Felicia", nullptr},        {8, "Bishamon", nullptr},
        {9, "Aulbath", "Rikuo"},       {10, "Sasquatch", nullptr},     {12, "Q-Bee", nullptr},
        {13, "Lei-Lei", "Hsien-Ko"},   {14, "Lilith", nullptr},        {15, "Jedah", nullptr},
        {kDarkGallon, "Dark Gallon", "Dark Talbain"}, {kOboroBishamon, "Oboro Bishamon", nullptr},
    };
    // The sets by what chooses them (vsav.h, kColours)
    constexpr const char *kSetNames[kColourSetsEach] = {"LP", "MP", "HP", "LK", "MK", "HK", "PP", "KK", "AUTO1", "AUTO2"};

    // Where the characters' colours are in the ROM (their sets and their effects' palettes), and the game's
    // own, kept to put back: a copy of the ROM from the first of those places to the end of the last
    struct Range {
        uint32_t from, to;
    };
    std::vector<Range> g_ranges;
    std::vector<uint8_t> g_original;
    uint32_t g_originalFrom = 0;
    bool g_applied = false, g_wasOn = false;

    // A ROM file as PalMod (the players' palette editor) changes it: the last of the set's program files, which
    // has all of the characters' colours, its words with their bytes swapped (RomSet::program has them as the
    // 68000 does)
    constexpr const char *kRomFile = "vm3j.10b";
    constexpr uint32_t kRomFileAt = 0x380000, kRomFileSize = 0x80000;

    uint32_t Long(uint32_t address) {
        const uint8_t *p = Program(address);
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
    }
    uint32_t SetsOf(int number) {
        return Long(kColourSets + 4u * static_cast<uint32_t>(number));
    }
    constexpr uint32_t kSetsSize = kColourSetSize * kColourSetsEach;
    constexpr uint32_t kEffectsSize = kEffectPalettes * kColoursInPalette * 2;
    bool InRanges(uint32_t address) {
        for (const Range &range : g_ranges) {
            if (address >= range.from && address < range.to) {
                return true;
            }
        }
        return false;
    }

    // A colour as the board has it (4 bits each) and as a file has it (8)
    std::string ColourText(uint16_t colour) {
        char text[8];
        snprintf(text, sizeof(text), "#%02X%02X%02X", ((colour >> 8) & 0x0F) * 0x11, ((colour >> 4) & 0x0F) * 0x11,
                 (colour & 0x0F) * 0x11);
        return text;
    }
    int HexDigit(char c) {
        return std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : std::isxdigit(static_cast<unsigned char>(c)) ? (std::tolower(c) - 'a' + 10) : -1;
    }
    // "#RGB" or "#RRGGBB" (each rounded to the board's 16 levels) to the board's 12 bits; -1 if neither
    int ParseColour(const std::string &word) {
        if (word.size() != 4 && word.size() != 7) {
            return -1;
        }
        if (word[0] != '#') {
            return -1;
        }
        int value = 0;
        for (int part = 0; part < 3; part++) {
            int level;
            if (word.size() == 4) {
                level = HexDigit(word[1 + part]);
            } else {
                const int high = HexDigit(word[1 + 2 * part]), low = HexDigit(word[2 + 2 * part]);
                level = high < 0 || low < 0 ? -1 : ((high << 4 | low) + 8) / 0x11;
            }
            if (level < 0) {
                return -1;
            }
            value = value << 4 | level;
        }
        return value;
    }
    std::string Upper(std::string text) {
        for (char &c : text) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return text;
    }

    // (beside settings.ini; none when the game was started without one)
    std::string FolderPath() {
        const std::string &settings = SettingsPath();
        const size_t slash = settings.find_last_of("/\\");
        return settings.empty() ? std::string() : (slash == std::string::npos ? std::string(".") : settings.substr(0, slash)) + "/" + kFolder;
    }

    // Reads a character's file, if it has one, into the ROM image; how many sets it changed
    int Load(const Character &who, const std::string &folder) {
        std::ifstream in;
        std::string path;
        for (const char *name : {who.name, who.abroad}) {
            if (name) {
                path = folder + "/" + name + ".txt";
                in.open(path);
                if (in) {
                    break;
                }
            }
        }
        if (!in) {
            return 0;
        }
        const uint32_t sets = SetsOf(who.number);
        int set = -1, palette = 0, changed = 0, number = 0;
        std::string line;
        auto complain = [&](const char *what) { fprintf(stderr, "%s, line %d: %s\n", path.c_str(), number, what); };
        while (std::getline(in, line)) {
            number++;
            // (a "#" with no hex digit after it starts a remark; "#12345" is a colour typed wrong)
            std::istringstream words(line);
            std::vector<std::string> list;
            for (std::string word; words >> word;) {
                if (word[0] == '#' && (word.size() == 1 || HexDigit(word[1]) < 0)) {
                    break;
                }
                list.push_back(word);
            }
            if (list.empty()) {
                continue;
            }
            if (list[0].front() == '[') {
                const std::string name = Upper(list[0].substr(1, list[0].find(']') - 1));
                set = -1;
                for (int n = 0; n < kColourSetsEach; n++) {
                    if (name == kSetNames[n]) {
                        set = n;
                    }
                }
                if (set < 0) {
                    complain("not a set's name (LP MP HP LK MK HK PP KK AUTO1 AUTO2)");
                }
                palette = 0;
                changed += set >= 0;
                continue;
            }
            if (set < 0) {
                continue; // (colours before a set's name, or under a name that is wrong: said already)
            }
            if (palette >= kPalettesInSet) {
                complain("a set has four lines of colours");
                continue;
            }
            if (list.size() > kColoursInPalette) {
                complain("more than 16 colours: the rest are left out");
            }
            for (size_t n = 0; n < list.size() && n < kColoursInPalette; n++) {
                if (list[n] == "-") {
                    continue; // (the game's own)
                }
                const int colour = ParseColour(list[n]);
                if (colour < 0) {
                    complain("not a colour (#RRGGBB, #RGB, or - for the game's own)");
                    continue;
                }
                uint8_t *p = Program(sets + static_cast<uint32_t>(set) * kColourSetSize +
                                     static_cast<uint32_t>(palette * kColoursInPalette + static_cast<int>(n)) * 2);
                // (the top four bits are the game's: kept)
                p[0] = static_cast<uint8_t>((p[0] & 0xF0) | (colour >> 8));
                p[1] = static_cast<uint8_t>(colour);
            }
            palette++;
        }
        return changed;
    }

    // Takes the colours a PalMod-changed copy of kRomFile in the folder has that the game's has not, where the
    // characters' colours are (and nothing else: the file holds the game's rules too); how many
    int LoadRomFile(const std::string &folder) {
        const std::string path = folder + "/" + kRomFile;
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return 0;
        }
        std::vector<uint8_t> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto at = [&](uint32_t address) { return file[(address - kRomFileAt) ^ 1]; };
        // (the tables of where the colours are must be the game's: it is this set's file)
        bool same = file.size() == kRomFileSize;
        for (uint32_t a = kColourSets; same && a < kEffectColours + kColourNumbers * 4; a++) {
            same = at(a) == *Program(a);
        }
        if (!same) {
            fprintf(stderr, "%s: not this set's %s (the Japanese set's, 970519), changed or not; left alone\n",
                    path.c_str(), kRomFile);
            return 0;
        }
        int colours = 0, elsewhere = 0;
        uint32_t firstElsewhere = 0;
        for (uint32_t a = kRomFileAt; a < kRomFileAt + kRomFileSize; a += 2) {
            uint8_t *p = Program(a);
            if (at(a) == p[0] && at(a + 1) == p[1]) {
                continue;
            }
            if (InRanges(a)) {
                p[0] = at(a);
                p[1] = at(a + 1);
                colours++;
            } else if (!elsewhere++) {
                firstElsewhere = a;
            }
        }
        if (elsewhere) {
            fprintf(stderr,
                    "%s: %d word%s changed outside the characters' colours (the first at %06X) left as the game's\n",
                    path.c_str(), elsewhere, elsewhere == 1 ? "" : "s", firstElsewhere);
        }
        return colours;
    }

    // The game's colours back, then the files' over them if the feature is on: kRomFile's, then the text files'
    void Apply() {
        if (g_original.empty()) {
            auto add = [](uint32_t from, uint32_t size) { g_ranges.push_back({from, from + size}); };
            for (int n = 0; n < kColourNumbers; n++) {
                add(SetsOf(n), kSetsSize);
            }
            for (int n = 0; n < kColourNumbers; n++) {
                add(Long(kEffectColours + 4u * static_cast<uint32_t>(n)), kEffectsSize);
            }
            uint32_t from = UINT32_MAX, to = 0;
            for (const Range &range : g_ranges) {
                from = std::min(from, range.from);
                to = std::max(to, range.to);
            }
            if (from < kRomFileAt || to > kRomFileAt + kRomFileSize || to > ProgramSize()) {
                fprintf(stderr, "custom colours: the ROM is not as expected; left alone\n");
                g_ranges.clear();
                g_applied = true;
                return;
            }
            g_originalFrom = from;
            g_original.assign(Program(from), Program(to));
        }
        std::copy(g_original.begin(), g_original.end(), Program(g_originalFrom));
        g_applied = true;
        g_wasOn = FeatureOn(kFeature);
        const std::string folder = FolderPath();
        if (!g_wasOn || folder.empty()) {
            return;
        }
        const int colours = LoadRomFile(folder);
        if (colours) {
            fprintf(stderr, "custom colours: %d colour%s from %s/%s\n", colours, colours == 1 ? "" : "s", folder.c_str(),
                    kRomFile);
        }
        int files = 0, sets = 0;
        for (const Character &who : kList) {
            const int changed = Load(who, folder);
            files += changed > 0;
            sets += changed;
        }
        if (files) {
            fprintf(stderr, "custom colours: %d set%s for %d character%s from %s\n", sets, sets == 1 ? "" : "s", files,
                    files == 1 ? "" : "s", folder.c_str());
        }
    }

    // The game's own colours (never the files'), a file a character, in palettes/original
    void WriteGameColours(Machine &) {
        if (!g_applied) {
            Apply();
        }
        const std::string folder = FolderPath();
        if (folder.empty() || g_original.empty()) {
            fprintf(stderr, "custom colours: no data folder to write to\n");
            return;
        }
        const std::string out = folder + "/" + kGameColours;
        std::error_code error;
        std::filesystem::create_directories(out, error);
        for (const Character &who : kList) {
            std::ofstream file(out + "/" + who.name + ".txt");
            file << "# " << who.name << (who.abroad ? std::string(" (") + who.abroad + ")" : std::string())
                 << ": the game's own colours.\n"
                    "# To change them, copy this file to the palettes folder and change the colours there. Each set\n"
                    "# is chosen by the buttons in its name ([PP]: two punches at once, [KK]: two kicks; [AUTO1] and\n"
                    "# [AUTO2]: chosen for auto guard) and has four lines of 16 colours; the character is drawn in\n"
                    "# the first line's, the others are its effects. The 16th colour of a line is never seen. A\n"
                    "# colour is #RRGGBB or #RGB, rounded to the board's 16 levels of each; - keeps the game's.\n"
                    "# Sets and lines that are left out keep the game's colours.\n";
            const uint32_t sets = SetsOf(who.number) - g_originalFrom;
            for (int set = 0; set < kColourSetsEach; set++) {
                file << "\n[" << kSetNames[set] << "]\n";
                for (int palette = 0; palette < kPalettesInSet; palette++) {
                    for (int n = 0; n < kColoursInPalette; n++) {
                        const uint8_t *p = &g_original[sets + static_cast<uint32_t>(set) * kColourSetSize +
                                                       static_cast<uint32_t>(palette * kColoursInPalette + n) * 2];
                        file << (n ? " " : "") << ColourText(static_cast<uint16_t>(p[0] << 8 | p[1]));
                    }
                    file << "\n";
                }
            }
        }
        fprintf(stderr, "custom colours: the game's colours written to %s\n", out.c_str());
    }
} // namespace

void PalettesBeforeFrame(Machine &) {
    if (!g_applied || FeatureOn(kFeature) != g_wasOn) {
        Apply();
    }
}

void RegisterPalettes() {
    TheGame().features.push_back({kFeature, "Display", "Custom colours",
                                  "Characters in the colours of the files in the \"palettes\" folder in the game's data "
                                  "folder: a vm3j.10b changed with PalMod, and text files, one a character (Morrigan.txt), "
                                  "which go before it. With no files the game's own colours. They are taken when a player is "
                                  "coloured: at the next round.",
                                  kFeatureEnhanced});
    TheGame().commands.push_back({"colours_reload", "Display", "Load colour files again",
                                  "Reads the palettes folder again, for files changed while the game runs. They are taken "
                                  "when a player is next coloured.",
                                  [](Machine &) { Apply(); }});
    TheGame().commands.push_back({"colours_write", "Display", "Write the game's colours",
                                  "Writes every character's own colours as files in palettes/original, to copy into the "
                                  "palettes folder and change.",
                                  WriteGameColours});
}

} // namespace cps2recomp

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Custom colours (colours.h): the files in the palettes folder read into the ROM image, the game's own colours
// kept to put back and written out, the editor's colours and its library of saved sets.
#include "colours.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace cps2recomp::colours {

namespace {
    constexpr const char *kFeature = "custom_colours";
    constexpr const char *kFolder = "palettes", *kGameColours = "original", *kLibrary = "library";

    Description g_description;

    // The game's own colours, kept to put back: a copy of the ROM from the first of their places to the end of
    // the last
    std::vector<Range> g_ranges;
    std::vector<uint8_t> g_original;
    uint32_t g_originalFrom = 0;
    bool g_applied = false, g_wasOn = false;

    int Sets() {
        return static_cast<int>(g_description.setNames.size());
    }
    int Palettes() {
        return g_description.palettesInSet;
    }
    // (a count as a word: "a set has four lines")
    const char *CountWord(int n, bool capital) {
        static const char *const lower[] = {"no", "one", "two", "three", "four", "five", "six", "seven", "eight"};
        static const char *const upper[] = {"No", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight"};
        return n >= 0 && n <= 8 ? (capital ? upper : lower)[n] : "several";
    }
    std::string SetNameList() {
        std::string list;
        for (const char *name : g_description.setNames) {
            list += (list.empty() ? "" : " ") + std::string(name);
        }
        return list;
    }
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
    // A line's words, up to a remark (a "#" with no hex digit after it; "#12345" is a colour typed wrong)
    std::vector<std::string> Words(const std::string &line) {
        std::istringstream words(line);
        std::vector<std::string> list;
        for (std::string word; words >> word;) {
            if (word[0] == '#' && (word.size() == 1 || HexDigit(word[1]) < 0)) {
                break;
            }
            list.push_back(word);
        }
        return list;
    }

    // (beside settings.ini; none when the game was started without one)
    std::string FolderPath() {
        const std::string &settings = SettingsPath();
        const size_t slash = settings.find_last_of("/\\");
        return settings.empty() ? std::string() : (slash == std::string::npos ? std::string(".") : settings.substr(0, slash)) + "/" + kFolder;
    }

    uint32_t ColourAddress(int number, int set, int line, int n) {
        return SetAddress(number, set) + static_cast<uint32_t>(line * kColoursInPalette + n) * 2;
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
        int set = -1, palette = 0, changed = 0, number = 0;
        std::string line;
        auto complain = [&](const std::string &what) { fprintf(stderr, "%s, line %d: %s\n", path.c_str(), number, what.c_str()); };
        while (std::getline(in, line)) {
            number++;
            const std::vector<std::string> list = Words(line);
            if (list.empty()) {
                continue;
            }
            if (list[0].front() == '[') {
                const std::string name = Upper(list[0].substr(1, list[0].find(']') - 1));
                set = -1;
                for (int n = 0; n < Sets(); n++) {
                    if (name == g_description.setNames[static_cast<size_t>(n)]) {
                        set = n;
                    }
                }
                if (set < 0) {
                    complain("not a set's name (" + SetNameList() + ")");
                }
                palette = 0;
                changed += set >= 0;
                continue;
            }
            if (set < 0) {
                continue; // (colours before a set's name, or under a name that is wrong: said already)
            }
            if (palette >= Palettes()) {
                complain(std::string("a set has ") + CountWord(Palettes(), false) + " lines of colours");
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
                SetColour(who.number, set, palette, static_cast<int>(n), static_cast<uint16_t>(colour));
            }
            palette++;
        }
        return changed;
    }

    // Takes the colours a PalMod-changed copy of the ROM file in the folder has that the game's has not, where the
    // characters' colours are (and nothing else: the file holds the game's rules too); how many
    int LoadRomFile(const std::string &folder) {
        const Description &d = g_description;
        if (!d.romFile) {
            return 0;
        }
        const std::string path = folder + "/" + d.romFile;
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return 0;
        }
        std::vector<uint8_t> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const uint32_t swap = d.romFileSwapped ? 1 : 0;
        auto at = [&](uint32_t address) { return file[(address - d.romFileAt) ^ swap]; };
        // (the stretch that must be the game's: it is this set's file)
        bool same = file.size() == d.romFileSize;
        for (uint32_t a = d.same.from; same && a < d.same.to; a++) {
            same = at(a) == *Program(a);
        }
        if (!same) {
            fprintf(stderr, "%s: not this set's %s (%s), changed or not; left alone\n", path.c_str(), d.romFile, d.romFileWhose);
            return 0;
        }
        int colours = 0, elsewhere = 0;
        uint32_t firstElsewhere = 0;
        for (uint32_t a = d.romFileAt; a < d.romFileAt + d.romFileSize; a += 2) {
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

    // The places of the colours found and the game's own kept (once); false if the ROM is not as expected
    bool Prepare() {
        if (!g_original.empty()) {
            return true;
        }
        if (!g_ranges.empty() || !g_description.ranges) {
            return false; // (found wrong once: said then)
        }
        g_ranges = g_description.ranges();
        uint32_t from = UINT32_MAX, to = 0;
        for (const Range &range : g_ranges) {
            from = std::min(from, range.from);
            to = std::max(to, range.to);
        }
        const Description &d = g_description;
        if (g_ranges.empty() || (d.romFile && (from < d.romFileAt || to > d.romFileAt + d.romFileSize)) || to > ProgramSize()) {
            fprintf(stderr, "custom colours: the ROM is not as expected; left alone\n");
            g_ranges.push_back({0, 0});
            return false;
        }
        g_originalFrom = from;
        g_original.assign(Program(from), Program(to));
        return true;
    }

    // The game's colours back, then the files' over them if the feature is on: the ROM file's, then the text files'
    void Apply() {
        g_applied = true;
        if (!Prepare()) {
            return;
        }
        std::copy(g_original.begin(), g_original.end(), Program(g_originalFrom));
        g_wasOn = FeatureOn(kFeature);
        const std::string folder = FolderPath();
        if (!g_wasOn || folder.empty()) {
            return;
        }
        const int colours = LoadRomFile(folder);
        if (colours) {
            fprintf(stderr, "custom colours: %d colour%s from %s/%s\n", colours, colours == 1 ? "" : "s", folder.c_str(),
                    g_description.romFile);
        }
        int files = 0, sets = 0;
        for (const Character &who : g_description.characters) {
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
        for (const Character &who : g_description.characters) {
            std::ofstream file(out + "/" + who.name + ".txt");
            file << "# " << who.name << (who.abroad ? std::string(" (") + who.abroad + ")" : std::string())
                 << ": the game's own colours.\n"
                 << g_description.originalHelp;
            for (int set = 0; set < Sets(); set++) {
                file << "\n[" << g_description.setNames[static_cast<size_t>(set)] << "]\n";
                for (int palette = 0; palette < Palettes(); palette++) {
                    for (int n = 0; n < kColoursInPalette; n++) {
                        file << (n ? " " : "") << ColourText(GameColour(who.number, set, palette, n));
                    }
                    file << "\n";
                }
            }
        }
        fprintf(stderr, "custom colours: the game's colours written to %s\n", out.c_str());
    }

    const Character *Find(int number) {
        for (const Character &one : g_description.characters) {
            if (one.number == number) {
                return &one;
            }
        }
        return nullptr;
    }
} // namespace

const Description &TheDescription() {
    return g_description;
}

const char *Name(int number) {
    const Character *who = Find(number);
    return who ? who->name : "";
}

uint32_t SetAddress(int number, int set) {
    return g_description.setAddress(number, set);
}

uint16_t Colour(int number, int set, int line, int n) {
    const uint8_t *p = Program(ColourAddress(number, set, line, n));
    return static_cast<uint16_t>(((p[0] << 8) | p[1]) & 0x0FFF);
}

uint16_t GameColour(int number, int set, int line, int n) {
    if (!Prepare()) {
        return Colour(number, set, line, n);
    }
    const uint8_t *p = &g_original[ColourAddress(number, set, line, n) - g_originalFrom];
    return static_cast<uint16_t>(((p[0] << 8) | p[1]) & 0x0FFF);
}

void SetColour(int number, int set, int line, int n, uint16_t colour) {
    uint8_t *p = Program(ColourAddress(number, set, line, n));
    p[0] = static_cast<uint8_t>((p[0] & 0xF0) | ((colour >> 8) & 0x0F)); // (the top four bits are the game's)
    p[1] = static_cast<uint8_t>(colour);
}

bool SaveColours(int number) {
    const Character *who = Find(number);
    const std::string folder = FolderPath();
    if (!who || folder.empty()) {
        return false;
    }
    // (into the file the character's colours were read from, if it is called by its name abroad)
    std::string path = folder + "/" + who->name + ".txt";
    if (who->abroad && !std::filesystem::exists(path) && std::filesystem::exists(folder + "/" + who->abroad + ".txt")) {
        path = folder + "/" + who->abroad + ".txt";
    }
    std::ostringstream text;
    bool any = false;
    for (int set = 0; set < Sets(); set++) {
        std::string lines;
        bool changed = false;
        for (int line = 0; line < Palettes(); line++) {
            bool same = true;
            for (int n = 0; n < kColoursInPalette; n++) {
                same = same && Colour(number, set, line, n) == GameColour(number, set, line, n);
            }
            if (same) {
                lines += "-\n"; // (the game's: or the PalMod file's)
                continue;
            }
            changed = true;
            for (int n = 0; n < kColoursInPalette; n++) {
                lines += (n ? " " : "") + ColourText(Colour(number, set, line, n));
            }
            lines += "\n";
        }
        if (changed) {
            text << "\n[" << g_description.setNames[static_cast<size_t>(set)] << "]\n" << lines;
            any = true;
        }
    }
    std::error_code error;
    if (!any) {
        std::filesystem::remove(path, error);
        return true;
    }
    std::filesystem::create_directories(folder, error);
    std::ofstream file(path);
    file << "# " << who->name << ": colours made with the colour editor (the sets and lines that are not the game's;\n"
         << "# a line that is - is the game's). The format is in palettes/original, \"Write the game's colours\".\n"
         << text.str();
    return static_cast<bool>(file);
}

namespace {
    std::string LibraryPath(int number) {
        const std::string folder = FolderPath();
        const char *name = Name(number);
        return folder.empty() || !*name ? std::string() : folder + "/" + kLibrary + "/" + name;
    }
    // (names in the order a person counts them: the numbers in them by their value)
    bool Before(const std::string &a, const std::string &b) {
        size_t i = 0, j = 0;
        while (i < a.size() && j < b.size()) {
            if (std::isdigit(static_cast<unsigned char>(a[i])) && std::isdigit(static_cast<unsigned char>(b[j]))) {
                size_t ei = i, ej = j;
                while (ei < a.size() && std::isdigit(static_cast<unsigned char>(a[ei]))) {
                    ei++;
                }
                while (ej < b.size() && std::isdigit(static_cast<unsigned char>(b[ej]))) {
                    ej++;
                }
                const unsigned long x = std::stoul(a.substr(i, ei - i)), y = std::stoul(b.substr(j, ej - j));
                if (x != y) {
                    return x < y;
                }
                i = ei;
                j = ej;
                continue;
            }
            const int x = std::tolower(static_cast<unsigned char>(a[i])), y = std::tolower(static_cast<unsigned char>(b[j]));
            if (x != y) {
                return x < y;
            }
            i++;
            j++;
        }
        return a.size() - i < b.size() - j;
    }
} // namespace

std::vector<std::string> SavedSets(int number) {
    std::vector<std::string> names;
    const std::string path = LibraryPath(number);
    std::error_code error;
    if (path.empty() || !std::filesystem::is_directory(path, error)) {
        return names;
    }
    for (const auto &entry : std::filesystem::directory_iterator(path, error)) {
        if (entry.is_regular_file(error) && entry.path().extension() == ".txt") {
            names.push_back(entry.path().stem().string());
        }
    }
    std::sort(names.begin(), names.end(), Before);
    return names;
}

bool SaveSet(int number, int set, std::string &saved) {
    const std::string path = LibraryPath(number);
    if (path.empty()) {
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(path, error);
    // (the next number after the character's name that is free)
    for (int n = 1;; n++) {
        saved = std::string(Name(number)) + " " + std::to_string(n);
        if (!std::filesystem::exists(path + "/" + saved + ".txt", error)) {
            break;
        }
    }
    std::ofstream file(path + "/" + saved + ".txt");
    file << "# " << Name(number) << ": a set of colours saved in the colour editor (" << g_description.setNames[static_cast<size_t>(set)]
         << "), to load into any of the character's sets.\n"
         << "# " << CountWord(Palettes(), true)
         << " lines of 16 colours: the character's own, then its effects'. A colour is #RRGGBB or #RGB;\n"
            "# - keeps the colour of the set it is loaded into.\n";
    for (int line = 0; line < Palettes(); line++) {
        for (int n = 0; n < kColoursInPalette; n++) {
            file << (n ? " " : "") << ColourText(Colour(number, set, line, n));
        }
        file << "\n";
    }
    return static_cast<bool>(file);
}

bool ReadSavedSet(int number, const std::string &saved, uint16_t colours[kMostPalettes][kColoursInPalette]) {
    const std::string path = LibraryPath(number);
    std::ifstream in(path + "/" + saved + ".txt");
    if (path.empty() || !in) {
        return false;
    }
    // (lines of colours, as in a character's file; a [name] line, if any, is passed over)
    int line = 0;
    for (std::string text; line < Palettes() && std::getline(in, text);) {
        const std::vector<std::string> list = Words(text);
        if (list.empty() || list[0].front() == '[') {
            continue;
        }
        for (size_t n = 0; n < list.size() && n < kColoursInPalette; n++) {
            const int colour = ParseColour(list[n]);
            if (colour >= 0) {
                colours[line][n] = static_cast<uint16_t>(colour);
            }
        }
        line++;
    }
    return line > 0;
}

bool RemoveSavedSet(int number, const std::string &saved) {
    const std::string path = LibraryPath(number);
    std::error_code error;
    return !path.empty() && std::filesystem::remove(path + "/" + saved + ".txt", error);
}

void EditorCheck(); // colour_editor.cpp
void RegisterEditor();

void BeforeFrame(Machine &) {
    EditorCheck();
    if (!g_applied || FeatureOn(kFeature) != g_wasOn) {
        Apply();
    }
}

void Register(const Description &description) {
    g_description = description;
    g_description.palettesInSet = std::clamp(description.palettesInSet, 1, kMostPalettes);
    TheGame().features.push_back({kFeature, "Display", "Custom colours", description.featureDescription, kFeatureEnhanced});
    RegisterEditor();
    // (the files' commands on the editor's page, after its own)
    TheGame().commands.push_back({"colours_reload", "Colour editor", "Load colour files again",
                                  "Reads the palettes folder again, for files changed while the game runs. They are taken "
                                  "when a player is next coloured.",
                                  [](Machine &) { Apply(); }});
    TheGame().commands.push_back({"colours_write", "Colour editor", "Write the game's colours",
                                  "Writes every character's own colours as files in palettes/original, to copy into the "
                                  "palettes folder and change.",
                                  WriteGameColours});
}

} // namespace cps2recomp::colours

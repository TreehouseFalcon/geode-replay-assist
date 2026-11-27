#include "NetUtils.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>
#include <charconv>
#include <string_view>

using namespace geode::prelude;

namespace NetUtils {
namespace {
    bool s_safeRecallDataLoaded = false;
    SafeRecallCache s_safeRecallEntries;

    // Ensure that the expected values in the sheet are actually positive ints //
    std::optional<int> parsePositiveInteger(std::string_view text) {
        if (text.empty() || text.front() < '1' || text.front() > '9') return std::nullopt;

        int value = 0;
        auto last = text.data() + text.size();
        auto [end, error] = std::from_chars(text.data(), last, value);
        if (error != std::errc{} || end != last) return std::nullopt;

        return value;
    }

    void appendSafeRecallRow(std::vector<std::string> const& columns, std::vector<SafeRecallEntry>& entries) {
        /*
            A = name
            D = level ID
            O = exact timed appearance label
        */
        if (columns.size() < 15 || columns[0].empty()) return;
        auto levelID = parsePositiveInteger(columns[3]);
        if (!levelID) return;

        std::string_view label = columns[14];
        LevelCategory category;
        int offset;
        if (label.starts_with("Daily #")) {
            category = LevelCategory::Daily;
            offset = 0;
            label.remove_prefix(7);
        } else if (label.starts_with("Weekly #")) {
            category = LevelCategory::Weekly;
            offset = 100000;
            label.remove_prefix(8);
        } else if (label.starts_with("Event #")) {
            category = LevelCategory::Event;
            offset = 200000;
            label.remove_prefix(7);
        } else {
            return;
        }

        // Reject whitespace, suffixes, signs, leading zeros, and category overflow.
        auto number = parsePositiveInteger(label);
        if (!number || *number > 100000) return;
        entries.push_back({columns[0], *levelID, category, offset + *number});
    }

    template <class RowConsumer>
    bool parseCSVRows(std::string_view csv, RowConsumer consumeRow) {
        if (csv.starts_with("\xEF\xBB\xBF")) csv.remove_prefix(3);
        std::vector<std::string> columns;
        std::string field;
        bool quoted = false;
        bool afterQuote = false;

        auto finishField = [&] {
            columns.push_back(std::move(field));
            field.clear();
            afterQuote = false;
        };
        auto finishRow = [&] {
            finishField();
            consumeRow(columns);
            columns.clear();
        };

        for (size_t i = 0; i < csv.size(); ++i) {
            char ch = csv[i];
            if (quoted) {
                if (ch == '"') {
                    if (i + 1 < csv.size() && csv[i + 1] == '"') {
                        field += '"';
                        ++i;
                    } else {
                        quoted = false;
                        afterQuote = true;
                    }
                } else {
                    field += ch;
                }
            } else if (ch == ',') {
                finishField();
            } else if (ch == '\n' || ch == '\r') {
                if (ch == '\r' && i + 1 < csv.size() && csv[i + 1] == '\n') ++i;
                finishRow();
            } else if (afterQuote) {
                return false;
            } else if (ch == '"') {
                if (!field.empty()) return false;
                quoted = true;
            } else {
                field += ch;
            }
        }
        if (quoted) return false;
        if (!columns.empty() || !field.empty() || afterQuote) finishRow();
        return true;
    }

    std::optional<std::vector<SafeRecallEntry>> parseSafeRecallCSV(std::string_view csv) {
        std::vector<SafeRecallEntry> entries;
        if (!parseCSVRows(csv, [&](auto const& columns) { appendSafeRecallRow(columns, entries); })) {
            return std::nullopt;
        }
        return entries;
    }

    void fetchSpreadsheet(
        async::TaskHolder<web::WebResponse>& request, std::string sheet, std::string range,
        geode::Function<void(std::string_view)> onLoaded
    ) {
        log::info("loading spreadsheet {} data", sheet);
        auto fetch = web::WebRequest().timeout(asp::Duration::fromSecs(30))
            .param("tqx", "out:csv").param("sheet", sheet).param("range", range);
        request.spawn(fetch.get(RECALL_SPREADSHEET_URL),
            [sheet, onLoaded = std::move(onLoaded)](web::WebResponse response) mutable {
                if (!response.ok()) {
                    log::warn("{} request failed (HTTP {}): {}", sheet, response.code(), response.errorMessage());
                    return;
                }
                auto text = response.string();
                if (!text) {
                    log::warn("could not read {} response: {}", sheet, text.unwrapErr());
                    return;
                }
                onLoaded(text.unwrap());
            });
    }

    void loadSafeRecallEntries() {
        static async::TaskHolder<web::WebResponse> request;
        fetchSpreadsheet(request, "The Safe", "A:O", [](std::string_view csv) {
            auto entries = parseSafeRecallCSV(csv);
            if (!entries || entries->empty()) {
                log::warn("Safe response contained no valid recall data or invalid CSV");
                return;
            }

            SafeRecallCache cache;
            for (auto& entry : *entries) {
                cache[entry.levelID].push_back(std::move(entry));
            }
            s_safeRecallEntries = std::move(cache);
            s_safeRecallDataLoaded = true;
            log::info("cached {} timed recall rows for {} levels", entries->size(), s_safeRecallEntries.size());
        });
    }
}

SafeRecallCache const& getSafeRecallEntries() {
    return s_safeRecallEntries;
}

bool isSafeRecallDataLoaded() {
    return s_safeRecallDataLoaded;
}

std::optional<int> getTimedIDForLevel(int levelID, std::optional<LevelCategory> category) {
    auto found = s_safeRecallEntries.find(levelID);
    if (found == s_safeRecallEntries.end()) return std::nullopt;

    std::optional<int> result;
    for (auto const& entry : found->second) {
        if (category && entry.levelCategory != *category) continue;
        if (!category && result && *result != entry.timedID) {
            return std::nullopt;
        }
        if (!result || entry.timedID > *result) result = entry.timedID;
    }
    return result;
}

$on_mod(Loaded) {
    queueInMainThread([] {
        loadSafeRecallEntries();
        loadGauntletRecallEntries();
    });
}

}

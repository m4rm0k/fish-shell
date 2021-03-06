// fish logging
#include "config.h"

#include "flog.h"
#include "global_safety.h"

#include "common.h"
#include "enum_set.h"
#include "parse_util.h"
#include "wildcard.h"

#include <vector>

namespace flog_details {

// Note we are relying on the order of global initialization within this file.
// It is important that 'all' be initialized before 'g_categories', because g_categories wants to
// append to all in the ctor.
/// This is not modified after initialization.
static std::vector<category_t *> s_all_categories;

/// When a category is instantiated it adds itself to the 'all' list.
category_t::category_t(const wchar_t *name, const wchar_t *desc, bool enabled)
    : name(name), description(desc), enabled(enabled) {
    s_all_categories.push_back(this);
}

/// Instantiate all categories.
/// This is deliberately leaked to avoid pointless destructor registration.
category_list_t *const category_list_t::g_instance = new category_list_t();

logger_t::logger_t() : file_(stderr) {}

owning_lock<logger_t> g_logger;

void logger_t::log1(const wchar_t *s) { std::fputws(s, file_); }
void logger_t::log1(const char *s) { std::fputs(s, file_); }

void logger_t::log1(wchar_t c) { std::fputwc(c, file_); }
void logger_t::log1(char c) { std::fputc(c, file_); }

void logger_t::log_fmt(const category_t &cat, const wchar_t *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    log1(cat.name);
    log1(": ");
    std::vfwprintf(file_, fmt, va);
    log1('\n');
    va_end(va);
}

void logger_t::log_fmt(const category_t &cat, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    log1(cat.name);
    log1(": ");
    std::vfprintf(file_, fmt, va);
    log1('\n');
    va_end(va);
}

}  // namespace flog_details

using namespace flog_details;

/// For each category, if its name matches the wildcard, set its enabled to the given sense.
static void apply_one_wildcard(const wcstring &wc_esc, bool sense) {
    wcstring wc = parse_util_unescape_wildcards(wc_esc);
    for (category_t *cat : s_all_categories) {
        if (wildcard_match(cat->name, wc)) {
            cat->enabled = sense;
        }
    }
}

void activate_flog_categories_by_pattern(const wcstring &inwc) {
    // Normalize underscores to dashes, allowing the user to be sloppy.
    wcstring wc = inwc;
    std::replace(wc.begin(), wc.end(), L'_', L'-');
    for (const wcstring &s : split_string(wc, L',')) {
        if (string_prefixes_string(s, L"-")) {
            apply_one_wildcard(s.substr(1), false);
        } else {
            apply_one_wildcard(s, true);
        }
    }
}

void set_flog_output_file(FILE *f) { g_logger.acquire()->set_file(f); }

std::vector<const category_t *> get_flog_categories() {
    std::vector<const category_t *> result(s_all_categories.begin(), s_all_categories.end());
    std::sort(result.begin(), result.end(), [](const category_t *a, const category_t *b) {
        return wcscmp(a->name, b->name) < 0;
    });
    return result;
}

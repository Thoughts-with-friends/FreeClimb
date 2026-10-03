#pragma once
//! Menu text in several languages.
//!
//! Built-in English is the base. Files
//! `FreeClimb_<language>.txt` in
//! `Interface/Translations` (UTF-8 or
//! UTF-16 with BOM, one `key<TAB>text`
//! per line) override it per language.




#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fc {
/// One built-in key and its English
/// text.
struct TranslationEntry {
  std::string_view key, value;
};
/// An available language: file id (e.g.
/// `"chinese"`) and display name.
struct TranslationLanguage {
  std::string id, name;
};
/// Canonical language id; legacy
/// numeric ids are accepted and unknown
/// ones map to `"english"`.
std::string normalizeLanguage(std::string_view language);

/// Result of listing the translation
/// directory.
enum class TranslationDirectoryStatus { ready, missing, unreadable };
/// File system access used by the
/// catalog; tests replace it.
class TranslationFileAccess {
public:
  virtual ~TranslationFileAccess() = default;
  /// List file names in `directory`.
  virtual TranslationDirectoryStatus
  list(const std::filesystem::path &directory,
       std::vector<std::filesystem::path> &filenames, std::string &error) const;
  /// Read at most `limit` bytes of
  /// `file` into `bytes`.
  virtual bool read(const std::filesystem::path &file, std::size_t limit,
                    std::string &bytes, std::string &error) const;
};

/// All menu texts with fallbacks.
///
/// # Lookup order
/// 1. The selected language file. 2.
/// The English file. 3. Built-in
/// English. 4. `"[Missing
/// translation]"` for unknown keys.
class TranslationCatalog {
public:
  /// Start with built-in English only.
  explicit TranslationCatalog(
      std::span<const TranslationEntry> englishDefaults);
  /// Reload all language files from
  /// `directory`.
  ///
  /// # Returns
  /// `false` if the directory cannot be
  /// read; previous texts are kept.
  bool reload(const std::filesystem::path &directory);
  /// `reload` with a custom file
  /// access.
  bool reload(const std::filesystem::path &directory,
              const TranslationFileAccess &files);
  /// Text of `key` in `language`. The
  /// pointer stays valid until the next
  /// reload.
  const char *text(std::string_view language, std::string_view key) const;
  /// Languages to offer: English,
  /// Chinese, then others by id.
  const std::vector<TranslationLanguage> &languages() const;
  /// Problems found by the last reload.
  const std::vector<std::string> &warnings() const;

private:
  using Texts = std::map<std::string, std::string, std::less<>>;
  Texts defaults_;
  std::map<std::string, Texts, std::less<>> external_;
  std::vector<TranslationLanguage> languages_;
  std::vector<std::string> warnings_;
  /// Recompute `languages_` after a
  /// reload.
  void rebuildLanguages();
};
} // namespace fc

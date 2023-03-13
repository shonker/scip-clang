#ifndef SCIP_CLANG_SYMBOL_FORMATTER_H
#define SCIP_CLANG_SYMBOL_FORMATTER_H

#include <optional>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"

#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/ADT/SmallVector.h"

#include "indexer/LlvmAdapter.h"
#include "indexer/Path.h"
#include "scip/scip.pb.h"

namespace clang {
class Decl;
class DeclContext;
class EnumConstantDecl;
class EnumDecl;
class NamedDecl;
class NamespaceDecl;
class TagDecl;
} // namespace clang

namespace llvm {
class raw_ostream;
}

namespace scip_clang {

using GetCanonicalPath =
    absl::FunctionRef<std::optional<RootRelativePathRef>(clang::FileID)>;

/// Type similar to \c scip::Descriptor but carrying \c string_view fields
/// instead to avoid redundant intermediate allocations.
struct DescriptorBuilder {
  std::string_view name;
  std::string_view disambiguator;
  scip::Descriptor::Suffix suffix;

  void formatTo(llvm::raw_ostream &) const;
};

/// Type similar to \c scip::Symbol but carrying \c string_view fields instead
/// to avoid redundant allocations.
///
/// Meant for transient use in constructing symbol strings, since a
/// \c scip::Index doesn't store any \c scip::Symbol values directly.
struct SymbolBuilder {
  std::string_view packageName;
  std::string_view packageVersion;
  llvm::SmallVector<DescriptorBuilder, 4> descriptors;

  /// Format a symbol string according to the standardized SCIP representation:
  /// https://github.com/sourcegraph/scip/blob/main/scip.proto#L101-L127
  void formatTo(std::string &) const;

  /// Format the symbol string for an entity, making use of the symbol string
  /// for its declaration context.
  ///
  /// For example, when constructing a symbol string for \c std::string_view,
  /// \p contextSymbol would be the symbol string for \c std,
  /// and \p descriptor would describe the \c string_view type.
  ///
  /// Since the standard formatting for SCIP symbols is prefix-based,
  /// this avoids the extra work of recomputing parent symbol strings.
  static std::string formatContextual(std::string_view contextSymbol,
                                      const DescriptorBuilder &descriptor);
};

class SymbolFormatter final {
  const clang::SourceManager &sourceManager;
  GetCanonicalPath getCanonicalPath;

  // Q: Should we allocate into an arena instead of having standalone
  // std::string values here?

  absl::flat_hash_map<LlvmToAbslHashAdapter<clang::SourceLocation>, std::string>
      locationBasedCache;
  absl::flat_hash_map<const clang::Decl *, std::string> declBasedCache;
  absl::flat_hash_map<LlvmToAbslHashAdapter<clang::FileID>, uint32_t>
      anonymousTypeCounters;
  std::string scratchBuffer;

public:
  SymbolFormatter(const clang::SourceManager &sourceManager,
                  GetCanonicalPath getCanonicalPath)
      : sourceManager(sourceManager), getCanonicalPath(getCanonicalPath),
        locationBasedCache(), declBasedCache(), scratchBuffer() {}
  SymbolFormatter(const SymbolFormatter &) = delete;
  SymbolFormatter &operator=(const SymbolFormatter &) = delete;

  std::string_view getMacroSymbol(clang::SourceLocation defLoc);

  std::optional<std::string_view>
  getEnumConstantSymbol(const clang::EnumConstantDecl *);

  std::optional<std::string_view> getEnumSymbol(const clang::EnumDecl *);

  std::optional<std::string_view> getNamedDeclSymbol(const clang::NamedDecl *);

  /// Returns nullopt for anonymous namespaces in files for which
  /// getCanonicalPath returns nullopt.
  std::optional<std::string_view>
  getNamespaceSymbol(const clang::NamespaceDecl *);

private:
  std::optional<std::string_view> getContextSymbol(const clang::DeclContext *);
  std::optional<std::string_view> getTagSymbol(const clang::TagDecl *);

  std::optional<std::string_view>
  getSymbolCached(const clang::Decl *,
                  absl::FunctionRef<std::optional<std::string>()>);

  /// Format the string to a buffer stored by `this` and return a view to it.
  template <typename... T>
  std::string_view formatTemporary(fmt::format_string<T...> fmt, T &&...args) {
    this->scratchBuffer.clear();
    fmt::format_to(std::back_inserter(this->scratchBuffer), fmt,
                   std::forward<T>(args)...);
    return std::string_view(this->scratchBuffer);
  }

  /// Format the string to a buffer stored by `this` and return a view to it.
  std::string_view formatTemporary(const clang::NamedDecl *);
};

} // namespace scip_clang

#endif // SCIP_CLANG_SYMBOL_FORMATTER_H
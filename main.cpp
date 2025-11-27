#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/Version.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Index/USRGeneration.h"
#include "clang/Tooling/AllTUsExecution.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/Signals.h"
#include <memory>
#include <mutex>
#include <map>


using namespace clang;
using namespace clang::ast_matchers;

template <class T, class Comp, class Alloc, class Predicate>
void discard_if(std::set<T, Comp, Alloc> &c, Predicate pred) {
  for (auto it{c.begin()}, end{c.end()}; it != end;) {
    if (pred(*it)) {
      it = c.erase(it);
    } else {
      ++it;
    }
  }
}

struct DeclLoc {
  DeclLoc() = default;
  DeclLoc(std::string Filename, unsigned Line)
      : Filename(std::move(Filename)), Line(Line) {}
  SmallString<128> Filename;
  unsigned Line;
};

struct DefInfo {
  size_t Definitions;
  size_t Uses;
  std::string Name;
  std::string Filename;
  unsigned Line;
  std::vector<DeclLoc> Declarations;
};

std::mutex Mutex;
std::map<std::string, DefInfo> AllDecls;

bool getUSRForDecl(const Decl *Decl, std::string &USR) {
  llvm::SmallVector<char, 128> Buff;

  if (index::generateUSRForDecl(Decl, Buff))
    return false;

  USR = std::string(Buff.data(), Buff.size());
  return true;
}

/// Returns all declarations that are not the definition of F
std::vector<DeclLoc> getDeclarations(const FunctionDecl *F,
                                     const SourceManager &SM) {
  std::vector<DeclLoc> Decls;
  for (const FunctionDecl *R : F->redecls()) {
    if (R->doesThisDeclarationHaveABody())
      continue;
    auto Begin = R->getSourceRange().getBegin();
    Decls.emplace_back(SM.getFilename(Begin).str(), SM.getSpellingLineNumber(Begin));
    SM.getFileManager().makeAbsolutePath(Decls.back().Filename);
  }
  return Decls;
}

class FunctionDeclMatchHandler : public MatchFinder::MatchCallback {
public:
  void finalize(const SourceManager &SM) {
    std::unique_lock<std::mutex> LockGuard(Mutex);

    for (const auto declaration: Defs) {
      std::string USR;
      if (!getUSRForDecl(declaration, USR))
        continue;

      const auto F = declaration->getDefinition();
      assert(F);
      auto it_inserted = AllDecls.emplace(std::move(USR), DefInfo{1, 0});
      if (!it_inserted.second) {
        it_inserted.first->second.Definitions++;
      }
      it_inserted.first->second.Name = F->getQualifiedNameAsString();

      auto Begin = F->getSourceRange().getBegin();
      it_inserted.first->second.Filename = SM.getFilename(Begin);
      it_inserted.first->second.Line = SM.getSpellingLineNumber(Begin);

      it_inserted.first->second.Declarations = getDeclarations(F, SM);

      // llvm::errs() << "saw definition: " << declaration->getNameAsString() << " USR: " << it_inserted.first->first <<
      //    " definitions: " << it_inserted.first->second.Definitions <<
      //    " uses: " << it_inserted.first->second.Uses << "\n";
    }

    for (const auto F: Uses) {
      std::string USR;
      if (!getUSRForDecl(F, USR))
        continue;
      auto it_inserted = AllDecls.emplace(std::move(USR), DefInfo{0, 1});
      if (!it_inserted.second) {
        it_inserted.first->second.Uses++;
      }
      // llvm::errs() << "saw usage: " << F->getNameAsString() << " USR: " << it_inserted.first->first <<
      //    " definitions: " << it_inserted.first->second.Definitions <<
      //    " uses: " << it_inserted.first->second.Uses << "\n";
    }
  }

  void handleUse(const ValueDecl *D, const SourceManager *SM) {
    auto *FD = dyn_cast<FunctionDecl>(D);
    if (!FD)
      return;

    // ignore uses of compiler-generated declarations
    if (FD->isImplicit())
        return;

    // ignore uses of declarations mentioned in a system header
    if (SM->isInSystemHeader(FD->getSourceRange().getBegin()))
      return;

    if (FD->isTemplateInstantiation()) {
      FD = FD->getTemplateInstantiationPattern();
      assert(FD);
    }

#if 0
    llvm::errs() << "Use ";
    FD->printName(llvm::errs());
    //llvm::errs() << " USR:" << USR;
    llvm::errs() << "\n";
#endif
    Uses.insert(FD->getCanonicalDecl());
  }
  void run(const MatchFinder::MatchResult &Result) override {
    if (const auto *F = Result.Nodes.getNodeAs<FunctionDecl>("fnDecl")) {
      if (!F->hasBody())
        return; // Ignore '= delete' and '= default' definitions.

      if (auto *Templ = F->getInstantiatedFromMemberFunction())
        F = Templ;

      if (F->isTemplateInstantiation()) {
        F = F->getTemplateInstantiationPattern();
        assert(F);
      }

      auto Begin = F->getSourceRange().getBegin();
      if (Result.SourceManager->isInSystemHeader(Begin))
        return;

      if (!Result.SourceManager->isWrittenInMainFile(Begin))
        return;

      auto *MD = dyn_cast<CXXMethodDecl>(F);
      if (MD) {
        if (MD->isVirtual()
        #if CLANG_VERSION_MAJOR >= 18
          && !MD->isPureVirtual()
        #else
          && !MD->isPure()
        #endif
          && MD->size_overridden_methods())
          return; // overriding method
        if (isa<CXXDestructorDecl>(MD))
          return; // We don't see uses of destructors.
      }

      if (F->isMain())
        return;
#if 0
      llvm::errs() << "FunctionDecl ";
      F->printName(llvm::errs());
      llvm::errs() << " USR:" << USR << "\n";
#endif
      Defs.insert(F->getCanonicalDecl());

      // __attribute__((constructor())) are always used
      if (F->hasAttr<ConstructorAttr>())
        handleUse(F, Result.SourceManager);

    } else if (const auto *R = Result.Nodes.getNodeAs<DeclRefExpr>("declRef")) {
      handleUse(R->getDecl(), Result.SourceManager);
    } else if (const auto *R =
                   Result.Nodes.getNodeAs<MemberExpr>("memberRef")) {
      handleUse(R->getMemberDecl(), Result.SourceManager);
    } else if (const auto *R = Result.Nodes.getNodeAs<CXXConstructExpr>(
                   "cxxConstructExpr")) {
      handleUse(R->getConstructor(), Result.SourceManager);
    } else if (const auto *R = Result.Nodes.getNodeAs<CXXNewExpr>("cxxNewExpr")) {
        if (const auto opNew = R->getOperatorNew())
            handleUse(opNew, Result.SourceManager);
        // a new expression calls operator delete on initialization failures
        if (const auto opDelete = R->getOperatorDelete())
            handleUse(opDelete, Result.SourceManager);
    } else if (const auto *R = Result.Nodes.getNodeAs<CXXDeleteExpr>("cxxDeleteExpr")) {
        if (const auto opDelete = R->getOperatorDelete())
            handleUse(opDelete, Result.SourceManager);
    }
  }

  std::set<const FunctionDecl *> Defs;
  std::set<const FunctionDecl *> Uses;
};

class XUnusedASTConsumer : public ASTConsumer {
public:
  XUnusedASTConsumer() {
    Matcher.addMatcher(
        functionDecl(isDefinition(), unless(isImplicit())).bind("fnDecl"),
        &Handler);
    Matcher.addMatcher(cxxNewExpr().bind("cxxNewExpr"), &Handler);
    Matcher.addMatcher(cxxDeleteExpr().bind("cxxDeleteExpr"), &Handler);
    Matcher.addMatcher(declRefExpr().bind("declRef"), &Handler);
    Matcher.addMatcher(memberExpr().bind("memberRef"), &Handler);
    Matcher.addMatcher(cxxConstructExpr().bind("cxxConstructExpr"), &Handler);
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    Matcher.matchAST(Context);
    Handler.finalize(Context.getSourceManager());
  }

private:
  FunctionDeclMatchHandler Handler;
  MatchFinder Matcher;
};

// For each source file provided to the tool, a new FrontendAction is created.
class XUnusedFrontendAction : public ASTFrontendAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance & /*CI*/,
                                                 StringRef /*File*/) override {
    return std::make_unique<XUnusedASTConsumer>();
  }
};

class XUnusedFrontendActionFactory : public tooling::FrontendActionFactory {
public:
  std::unique_ptr<FrontendAction> create() override { return std::make_unique<XUnusedFrontendAction>(); }
};

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  const char *Overview = R"(
  xunused is tool to find unused functions and methods across a whole C/C++ project.
  )";

  tooling::ExecutorName.setInitialValue("all-TUs");
#if 1
  auto Executor = clang::tooling::createExecutorFromCommandLineArgs(
      argc, argv, llvm::cl::getGeneralCategory(), Overview);
  if (!Executor) {
    llvm::errs() << llvm::toString(Executor.takeError()) << "\n";
    return 1;
  }
  auto Err =
      Executor->get()->execute(std::unique_ptr<XUnusedFrontendActionFactory>(
          new XUnusedFrontendActionFactory()));
#else
  static llvm::cl::OptionCategory XUnusedCategory("xunused options");
  CommonOptionsParser op(argc, argv, XUnusedCategory);
  AllTUsToolExecutor > Executor(op.getCompilations(), /*ThreadCount=*/0);
  auto Err = Executor.execute(std::unique_ptr<XUnusedFrontendActionFactory>(
      new XUnusedFrontendActionFactory()));
#endif

  if (Err) {
    llvm::errs() << llvm::toString(std::move(Err)) << "\n";
  }

  for (auto &KV : AllDecls) {
    DefInfo &I = KV.second;
    if (I.Definitions > 0 && I.Uses == 0) {
      llvm::errs() << I.Filename << ":" << I.Line << ": warning:"
                   << " Function '" << I.Name << "' is unused\n";
      for (auto &D : I.Declarations) {
        llvm::errs() << D.Filename << ":" << D.Line << ": note:"
                     << " declared here\n";
      }
    }
  }
}

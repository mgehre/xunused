#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
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

struct DeclLoc {
  DeclLoc() = default;
  DeclLoc(std::string Filename, unsigned Line)
      : Filename(std::move(Filename)), Line(Line) {}
  SmallString<128> Filename;
  unsigned Line;
};

struct DefInfo {
  const FunctionDecl *Definition;
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
    Decls.emplace_back(SM.getFilename(Begin), SM.getSpellingLineNumber(Begin));
    SM.getFileManager().makeAbsolutePath(Decls.back().Filename);
  }
  return Decls;
}

class FunctionDeclMatchHandler : public MatchFinder::MatchCallback {
public:
  void finalize(const SourceManager &SM) {
    std::unique_lock<std::mutex> LockGuard(Mutex);

    std::vector<const FunctionDecl *> UnusedDefs;

    std::set_difference(Defs.begin(), Defs.end(), Uses.begin(), Uses.end(),
                        std::back_inserter(UnusedDefs));

    for (auto *F : UnusedDefs) {
      F = F->getDefinition();
      assert(F);
      std::string USR;
      if (!getUSRForDecl(F, USR))
        continue;
      // llvm::errs() << "UnusedDefs: " << USR << "\n";
      auto it_inserted = AllDecls.emplace(std::move(USR), DefInfo{F, 0});
      if (!it_inserted.second) {
        it_inserted.first->second.Definition = F;
      }
      it_inserted.first->second.Name = F->getQualifiedNameAsString();

      auto Begin = F->getSourceRange().getBegin();
      it_inserted.first->second.Filename = SM.getFilename(Begin);
      it_inserted.first->second.Line = SM.getSpellingLineNumber(Begin);

      it_inserted.first->second.Declarations = getDeclarations(F, SM);
    }

    std::vector<const FunctionDecl *> ExternalUses;

    std::set_difference(Uses.begin(), Uses.end(), Defs.begin(), Defs.end(),
                        std::back_inserter(ExternalUses));

    /*for (auto *F : Uses) {
        llvm::errs() << "Uses: " << F << " " << F->getNameAsString() << "\n";
    }
    for (auto *F : Defs) {
        llvm::errs() << "Defs: " << F << " " << F->getNameAsString() << "\n";
    }*/

    for (auto *F : ExternalUses) {
      // llvm::errs() << "ExternalUses: " << F->getNameAsString() << "\n";
      std::string USR;
      if (!getUSRForDecl(F, USR))
        continue;
      // llvm::errs() << "ExternalUses: " << USR << "\n";
      auto it_inserted = AllDecls.emplace(std::move(USR), DefInfo{nullptr, 1});
      if (!it_inserted.second) {
        it_inserted.first->second.Uses++;
      }
    }
  }

  void handleUse(const ValueDecl *D, const SourceManager *SM) {
    auto *FD = dyn_cast<FunctionDecl>(D);
    if (!FD)
      return;

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
        if (MD->isVirtual() && !MD->isPure() && MD->size_overridden_methods())
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
    } else if (const auto *R = Result.Nodes.getNodeAs<DeclRefExpr>("declRef")) {
      handleUse(R->getDecl(), Result.SourceManager);
    } else if (const auto *R =
                   Result.Nodes.getNodeAs<MemberExpr>("memberRef")) {
      handleUse(R->getMemberDecl(), Result.SourceManager);
    } else if (const auto *R = Result.Nodes.getNodeAs<CXXConstructExpr>(
                   "cxxConstructExpr")) {
      handleUse(R->getConstructor(), Result.SourceManager);
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
      argc, argv, llvm::cl::GeneralCategory, Overview);
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
    if (I.Definition && I.Uses == 0) {
      llvm::errs() << I.Filename << ":" << I.Line << ": warning:"
                   << " Function '" << I.Name << "' is unused\n";
      for (auto &D : I.Declarations) {
        llvm::errs() << D.Filename << ":" << D.Line << ": note:"
                     << " declared here\n";
      }
    }
  }
}

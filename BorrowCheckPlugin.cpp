#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/ParentMapContext.h" // For getParents
#include "clang/AST/DeclCXX.h"          // For CXXRecordDecl, CXXConstructorDecl
#include "clang/AST/ExprCXX.h"          // For CXXConstructExpr, MemberExpr
#include "clang/AST/Decl.h"             // For VarDecl, ValueDecl
#include "clang/AST/Stmt.h"             // For CompoundStmt

#include <unordered_map>
#include <vector>
#include <string>

using namespace clang;

// Represents the borrow state of a variable
struct BorrowState
{
  bool mutablyBorrowed = false;
  int immutablyBorrowed = 0;
};

// Manages the borrow state and scope for variables
class BorrowContext
{
  std::unordered_map<std::string, BorrowState> currentBorrowStates;
  std::vector<std::unordered_map<std::string, BorrowState>> scopeStack;
  ASTContext &astContext;
  DiagnosticsEngine &DE;

public:
  explicit BorrowContext(ASTContext &ctx)
      : astContext(ctx), DE(ctx.getDiagnostics()) {}

  // Generates a unique key for a variable declaration based on its source location
  static std::string getKeyForDecl(const ValueDecl *decl, ASTContext &ctx)
  {
    if (!decl)
      return "<unknown_decl_key>";
    return decl->getLocation().printToString(ctx.getSourceManager());
  }

  void enterScope()
  {
    scopeStack.push_back(currentBorrowStates);
  }

  void exitScope()
  {
    if (!scopeStack.empty())
    {
      currentBorrowStates = scopeStack.back();
      scopeStack.pop_back();
    }
  }

  // Adds a new variable to be tracked
  void addTrackedVariable(const std::string &varKey)
  {
    currentBorrowStates[varKey] = BorrowState();
  }

  // Records an immutable borrow and checks for violations
  void recordImmutableBorrow(const std::string &varKey, const std::string &varName, SourceLocation reportLoc)
  {
    BorrowState *state = &currentBorrowStates[varKey]; // Assumes varKey exists from addTrackedVariable

    // If state is not found, it means the variable is not being tracked
    if (state == nullptr)
    {
      unsigned ID = DE.getCustomDiagID(DiagnosticsEngine::Error,
                                       "Variable '%0' is not being tracked");
      DE.Report(reportLoc, ID) << varName;
      return;
    }

    state->immutablyBorrowed++;
    if (state->mutablyBorrowed)
    {
      unsigned ID = DE.getCustomDiagID(DiagnosticsEngine::Error,
                                       "Cannot immutably borrow '%0' while it is mutably borrowed");
      DE.Report(reportLoc, ID) << varName;
    }
  }

  // Records a mutable borrow and checks for violations
  void recordMutableBorrow(const std::string &varKey, const std::string &varName, SourceLocation reportLoc)
  {
    BorrowState *state = &currentBorrowStates[varKey]; // Similar assumption as above

    // If state is not found, it means the variable is not being tracked
    if (state == nullptr)
    {
      unsigned ID = DE.getCustomDiagID(DiagnosticsEngine::Error,
                                       "Variable '%0' is not being tracked");
      DE.Report(reportLoc, ID) << varName;
      return;
    }

    state->mutablyBorrowed = true;
    if (state->immutablyBorrowed > 0)
    {
      unsigned ID = DE.getCustomDiagID(DiagnosticsEngine::Error,
                                       "Cannot mutably borrow '%0' while it is immutably borrowed");
      DE.Report(reportLoc, ID) << varName;
    }
  }

  void clear()
  {
    currentBorrowStates.clear();
    scopeStack.clear();
  }
};

namespace
{

  class BorrowCheckerVisitor : public RecursiveASTVisitor<BorrowCheckerVisitor>
  {
    ASTContext &Context;
    BorrowContext &borrowContext;

  public:
    explicit BorrowCheckerVisitor(ASTContext &ctx, BorrowContext &bc)
        : Context(ctx), borrowContext(bc) {}

    bool VisitCXXConstructExpr(CXXConstructExpr *expr)
    {
      if (!expr)
        return true;
      const CXXConstructorDecl *ctor = expr->getConstructor();
      if (!ctor)
        return true;
      const CXXRecordDecl *record = ctor->getParent();
      if (!record)
        return true;

      std::string className = record->getNameAsString();

      if (className == "Unique")
      {
        const VarDecl *initializedVarDecl = nullptr;
        // Try to find the VarDecl this constructor is initializing
        auto parents = Context.getParents(*expr);
        for (const auto &parent : parents)
        {
          if (const auto *vd = parent.get<VarDecl>())
          {
            initializedVarDecl = vd;
            break;
          }
          // Handle cases like Unique u = Unique()
          if (const auto *ile = parent.get<InitListExpr>())
          {
            auto ileParents = Context.getParents(*ile);
            for (const auto &ileParent : ileParents)
            {
              if (const auto *vd_ile = ileParent.get<VarDecl>())
              {
                initializedVarDecl = vd_ile;
                break;
              }
            }
            if (initializedVarDecl)
              break;
          }
        }

        if (initializedVarDecl)
        {
          std::string varKey = BorrowContext::getKeyForDecl(initializedVarDecl, Context);
          borrowContext.addTrackedVariable(varKey);
        }
      }
      return true;
    }

    bool VisitCallExpr(CallExpr *expr)
    {
      if (!expr)
        return true;
      Expr *callee = expr->getCallee();
      if (!callee)
        return true;

      auto *memberCall = dyn_cast<MemberExpr>(callee->IgnoreParenCasts());
      if (!memberCall)
        return true;

      auto *methodDecl = dyn_cast<CXXMethodDecl>(memberCall->getMemberDecl());
      if (!methodDecl)
        return true;

      std::string methodName = methodDecl->getNameAsString();
      if (methodName != "borrow" && methodName != "borrow_mut")
        return true;

      Expr *base = memberCall->getBase()->IgnoreParenCasts();
      auto *declRef = dyn_cast<DeclRefExpr>(base);
      if (!declRef)
        return true;

      const ValueDecl *varValueDecl = declRef->getDecl();
      if (!varValueDecl)
        return true;

      std::string varName = varValueDecl->getNameAsString();
      std::string varKey = BorrowContext::getKeyForDecl(varValueDecl, Context);
      SourceLocation reportLoc = expr->getExprLoc();

      if (methodName == "borrow")
      {
        borrowContext.recordImmutableBorrow(varKey, varName, reportLoc);
      }
      else // borrow_mut
      {
        borrowContext.recordMutableBorrow(varKey, varName, reportLoc);
      }
      return true;
    }

    bool VisitDeclRefExpr(DeclRefExpr *expr)
    {
      // Currently unused, but can be extended for use-after-free checks
      return true;
    }

    bool TraverseCompoundStmt(CompoundStmt *stmt)
    {
      borrowContext.enterScope();
      RecursiveASTVisitor::TraverseCompoundStmt(stmt);
      borrowContext.exitScope();
      return true;
    }
  };

  class BorrowCheckConsumer : public ASTConsumer
  {
    BorrowContext borrowContext;
    ASTContext &astContext;

  public:
    explicit BorrowCheckConsumer(ASTContext &Context)
        : borrowContext(Context), astContext(Context)
    {
      DiagnosticsEngine &DE = Context.getDiagnostics();
      unsigned ID = DE.getCustomDiagID(DiagnosticsEngine::Warning,
                                       "BorrowCheckPlugin is running");
      DE.Report(ID);
    }

    void HandleTranslationUnit(ASTContext &Context) override
    {
      borrowContext.clear(); // Clear the context
      BorrowCheckerVisitor visitor(astContext, borrowContext);
      visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
  };

  class BorrowCheckAction : public PluginASTAction
  {
  protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                   llvm::StringRef) override
    {
      return std::make_unique<BorrowCheckConsumer>(CI.getASTContext());
    }

    bool ParseArgs(const CompilerInstance &CI,
                   const std::vector<std::string> &args) override
    {
      return true;
    }
  };

} // namespace

static FrontendPluginRegistry::Add<BorrowCheckAction>
    X("borrow-check", "Rust-like borrow checking analysis");
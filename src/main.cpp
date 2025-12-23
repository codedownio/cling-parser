#include <iostream>
#include <string>

#include "cling/Interpreter/Interpreter.h"
#include "cling/Interpreter/Transaction.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclGroup.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/Support/Casting.h"

#include <set>

using interpreter_ptr = std::unique_ptr<cling::Interpreter>;

interpreter_ptr build_interpreter(int argc, char** argv)
{
    int interpreter_argc = argc + 1;
    const char** interpreter_argv = new const char*[interpreter_argc];
    interpreter_argv[0] = "minimal-parser";

    // Copy all arguments in the new array excepting the process name.
    for (int i = 1; i < argc; i++)
    {
        interpreter_argv[i] = argv[i];
    }

    std::string include_dir = std::string(LLVM_DIR) + std::string("/include");
    interpreter_argv[interpreter_argc - 1] = include_dir.c_str();

    interpreter_ptr interp_ptr = std::make_unique<cling::Interpreter>(interpreter_argc, interpreter_argv, LLVM_DIR);

    delete[] interpreter_argv;
    return interp_ptr;
}

int main(int argc, char* argv[])
{
    std::cout << "BEGINNING OF MAIN" << std::endl;

    auto interp = build_interpreter(argc, argv);

    // Test code with actual declarations to parse
    std::string code = R"(
#include <iostream>

using namespace std;

int x = 42;

void myFunction(int param) {
    return;
}

class MyClass {
public:
    int member;
    void method();
 };

cout << "hello" << endl;
)";

    // Try basic Cling parsing
    cling::Transaction* T = nullptr;
    std::cerr << "About to parse code: " << code << std::endl;
    cling::Interpreter::CompilationResult result = interp->parse(code, &T);
    std::cerr << "Parse completed, result: " << result << ", transaction: " << (void*)T << std::endl;

    if (T && result == cling::Interpreter::kSuccess) {
        std::cerr << "Transaction details:" << std::endl;

        // Get source manager for location information
        clang::SourceManager& SM = interp->getCI()->getSourceManager();

        // Count declarations and filter for input source only
        size_t totalDeclCount = 0;
        size_t inputDeclCount = 0;
        std::set<clang::Decl*> seenDecls; // Deduplicate declarations

        // Iterate through the declaration groups in the transaction
        for (auto it = T->decls_begin(); it != T->decls_end(); ++it) {
            const cling::Transaction::DelayCallInfo& callInfo = *it;
            clang::DeclGroupRef DGR = callInfo.m_DGR;

            // Iterate through individual declarations in this group
            for (auto declIt = DGR.begin(); declIt != DGR.end(); ++declIt) {
                clang::Decl* decl = *declIt;
                totalDeclCount++;

                if (decl) {
                    clang::SourceLocation loc = decl->getLocation();

                    // Check if this declaration is from our input (not from system headers)
                    if (loc.isValid()) {
                        clang::FileID fileID = SM.getFileID(loc);
                        const clang::FileEntry* fileEntry = SM.getFileEntryForID(fileID);

                        // Input code shows up as special "input_line_X" files or similar
                        std::string filename = "";
                        if (fileEntry) {
                            filename = std::string(fileEntry->getName());
                        } else {
                            // For memory buffers/input, use the buffer name
                            if (auto bufferPtr = SM.getBufferOrNone(fileID)) {
                                filename = bufferPtr->getBufferIdentifier().str();
                            }
                        }

                        // Filter: ONLY show declarations from actual input lines (much more restrictive)
                        bool isFromInput = filename.find("input_line_") != std::string::npos;

                        // Skip if we've already seen this declaration (deduplicate)
                        if (isFromInput && seenDecls.find(decl) == seenDecls.end()) {
                            seenDecls.insert(decl);

                            inputDeclCount++;

                            // Get line/column information for start and end
                            clang::SourceRange sourceRange = decl->getSourceRange();
                            clang::SourceLocation startLoc = sourceRange.getBegin();
                            clang::SourceLocation endLoc = sourceRange.getEnd();

                            clang::PresumedLoc startPresumedLoc = SM.getPresumedLoc(startLoc);
                            clang::PresumedLoc endPresumedLoc = SM.getPresumedLoc(endLoc);

                            std::cerr << "  Declaration from input:" << std::endl;
                            std::cerr << "    Type: " << decl->getDeclKindName() << std::endl;
                            std::cerr << "    Pointer: " << (void*)decl << std::endl;
                            std::cerr << "    Start: " << filename;
                            if (startPresumedLoc.isValid()) {
                                std::cerr << " " << startPresumedLoc.getLine()
                                          << ":" << startPresumedLoc.getColumn();
                            }
                            std::cerr << std::endl;
                            std::cerr << "    End: " << filename;
                            if (endPresumedLoc.isValid()) {
                                std::cerr << " " << endPresumedLoc.getLine()
                                          << ":" << endPresumedLoc.getColumn();
                            }
                            std::cerr << std::endl;

                            // Try to get more specific information
                            if (clang::NamedDecl* namedDecl = llvm::dyn_cast<clang::NamedDecl>(decl)) {
                                std::cerr << "    Name: " << namedDecl->getNameAsString() << std::endl;
                            }

                            // Show additional info for function declarations
                            if (clang::FunctionDecl* funcDecl = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
                                std::cerr << "    Function return type: "
                                         << funcDecl->getReturnType().getAsString() << std::endl;
                                std::cerr << "    Number of parameters: "
                                         << funcDecl->getNumParams() << std::endl;
                                if (funcDecl->hasBody()) {
                                    std::cerr << "    Has body: yes" << std::endl;
                                } else {
                                    std::cerr << "    Has body: no (declaration only)" << std::endl;
                                }
                            }

                            // Show additional info for variable declarations
                            if (clang::VarDecl* varDecl = llvm::dyn_cast<clang::VarDecl>(decl)) {
                                std::cerr << "    Variable type: "
                                         << varDecl->getType().getAsString() << std::endl;
                            }

                            // Show additional info for class/struct declarations
                            if (clang::CXXRecordDecl* recordDecl = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
                                std::cerr << "    Record type: " << (recordDecl->isClass() ? "class" : "struct") << std::endl;
                                if (recordDecl->isCompleteDefinition()) {
                                    std::cerr << "    Complete definition: yes" << std::endl;
                                } else {
                                    std::cerr << "    Complete definition: no (forward declaration)" << std::endl;
                                }
                                std::cerr << "    Definition pointer: " << (void*)recordDecl->getDefinition() << std::endl;
                                std::cerr << "    Is this definition: " << (recordDecl->isThisDeclarationADefinition() ? "yes" : "no") << std::endl;
                            }

                            // Show additional info for field declarations
                            if (clang::FieldDecl* fieldDecl = llvm::dyn_cast<clang::FieldDecl>(decl)) {
                                std::cerr << "    Field type: "
                                         << fieldDecl->getType().getAsString() << std::endl;
                            }

                            std::cerr << std::endl;
                        }
                    }
                }
            }
        }

        std::cerr << "Summary:" << std::endl;
        std::cerr << "  Total declarations: " << totalDeclCount << std::endl;
        std::cerr << "  Declarations from input: " << inputDeclCount << std::endl;

    } else {
        std::cerr << "No valid transaction or parsing failed (result: " << result << ")" << std::endl;
    }

    return 0;
}

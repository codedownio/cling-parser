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
#include <sstream>
#include <iomanip>
#include <fstream>

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
    std::string code((std::istreambuf_iterator<char>(std::cin)),
                     std::istreambuf_iterator<char>());

    auto interp = build_interpreter(argc, argv);

    // Try basic Cling parsing
    cling::Transaction* T = nullptr;
    cling::Interpreter::CompilationResult result = interp->parse(code, &T);

    std::cout << "[";
    bool first = true;

    if (T && result == cling::Interpreter::kSuccess) {
        // Get source manager for location information
        clang::SourceManager& SM = interp->getCI()->getSourceManager();

        std::set<clang::Decl*> seenDecls; // Deduplicate declarations

        // Iterate through the declaration groups in the transaction
        for (auto it = T->decls_begin(); it != T->decls_end(); ++it) {
            const cling::Transaction::DelayCallInfo& callInfo = *it;
            clang::DeclGroupRef DGR = callInfo.m_DGR;

            // Iterate through individual declarations in this group
            for (auto declIt = DGR.begin(); declIt != DGR.end(); ++declIt) {
                clang::Decl* decl = *declIt;

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

                        // Filter: ONLY show declarations from actual input lines
                        bool isFromInput = filename.find("input_line_") != std::string::npos;

                        // Skip if we've already seen this declaration (deduplicate)
                        if (isFromInput && seenDecls.find(decl) == seenDecls.end()) {
                            seenDecls.insert(decl);

                            // Get line/column information for start and end
                            clang::SourceRange sourceRange = decl->getSourceRange();
                            clang::SourceLocation startLoc = sourceRange.getBegin();
                            clang::SourceLocation endLoc = sourceRange.getEnd();

                            clang::PresumedLoc startPresumedLoc = SM.getPresumedLoc(startLoc);
                            clang::PresumedLoc endPresumedLoc = SM.getPresumedLoc(endLoc);

                            if (!first) {
                                std::cout << ",";
                            }
                            first = false;

                            std::cout << "{";
                            std::cout << "\"type\":\"" << decl->getDeclKindName() << "\"";

                            if (startPresumedLoc.isValid()) {
                                std::cout << ",\"start_line\":" << startPresumedLoc.getLine();
                                std::cout << ",\"start_ch\":" << startPresumedLoc.getColumn();
                            } else {
                                std::cout << ",\"start_line\":null";
                                std::cout << ",\"start_ch\":null";
                            }

                            if (endPresumedLoc.isValid()) {
                                std::cout << ",\"end_line\":" << endPresumedLoc.getLine();
                                std::cout << ",\"end_ch\":" << endPresumedLoc.getColumn();
                            } else {
                                std::cout << ",\"end_line\":null";
                                std::cout << ",\"end_ch\":null";
                            }

                            std::cout << "}";
                        }
                    }
                }
            }
        }
    }

    std::cout << "]" << std::endl;

    return 0;
}

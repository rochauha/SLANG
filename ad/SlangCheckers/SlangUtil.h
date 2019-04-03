//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
//===----------------------------------------------------------------------===//
// The Util class has general purpose methods.
//===----------------------------------------------------------------------===//

#ifndef LLVM_SLANGUTIL_H
#define LLVM_SLANGUTIL_H

#include <string>
#include "llvm/Support/Process.h"

// TRACE < DEBUG < INFO < EVENT < ERROR < FATAL
#define SLANG_TRACE_LEVEL 10
#define SLANG_DEBUG_LEVEL 20
#define SLANG_INFO_LEVEL 30
#define SLANG_EVENT_LEVEL 40
#define SLANG_ERROR_LEVEL 50
#define SLANG_FATAL_LEVEL 60

// The macros for the five logging levels.
// TRACE < DEBUG < INFO < EVENT < ERROR < FATAL

#define SLANG_TRACE(XX)                                                                            \
    if (slang::Util::LogLevel <= SLANG_TRACE_LEVEL) {                                              \
        llvm::errs() << "\n  " << slang::Util::getDateTimeString() << ": TRACE ("                  \
                     << SLANG_TRACE_LEVEL << "):" << __FILE__ << ":" << __func__                   \
                     << "():" << __LINE__ << ":\n"                                                 \
                     << XX << "\n";                                                                        \
    }

#define SLANG_DEBUG(XX)                                                                            \
    if (slang::Util::LogLevel <= SLANG_DEBUG_LEVEL) {                                              \
        llvm::errs() << "\n  " << slang::Util::getDateTimeString() << ": DEBUG ("                  \
                     << SLANG_DEBUG_LEVEL << "):" << __FILE__ << ":" << __func__                   \
                     << "():" << __LINE__ << ":\n"                                                 \
                     << XX << "\n";                                                                        \
    }

#define SLANG_INFO(XX)                                                                             \
    if (slang::Util::LogLevel <= SLANG_INFO_LEVEL) {                                               \
        llvm::errs() << "\n  " << slang::Util::getDateTimeString() << ": INFO  ("                  \
                     << SLANG_INFO_LEVEL << "):" << __FILE__ << ":" << __func__                    \
                     << "():" << __LINE__ << ":\n"                                                 \
                     << XX << "\n";                                                                        \
    }

#define SLANG_EVENT(XX)                                                                            \
    if (slang::Util::LogLevel <= SLANG_EVENT_LEVEL) {                                              \
        llvm::errs() << "\n  " << slang::Util::getDateTimeString() << ": EVENT ("                  \
                     << SLANG_EVENT_LEVEL << "):" << __FILE__ << ":" << __func__                   \
                     << "():" << __LINE__ << ":\n"                                                 \
                     << XX << "\n";                                                                        \
    }

#define SLANG_ERROR(XX)                                                                            \
    if (slang::Util::LogLevel <= SLANG_ERROR_LEVEL) {                                              \
        llvm::errs() << "\n  " << slang::Util::getDateTimeString() << ": ERROR ("                  \
                     << SLANG_ERROR_LEVEL << "):" << __FILE__ << ":" << __func__                   \
                     << "():" << __LINE__ << ":\n"                                                 \
                     << XX << "\n";                                                                        \
    }

#define SLANG_FATAL(XX)                                                                            \
    if (slang::Util::LogLevel <= SLANG_FATAL_LEVEL) {                                              \
        llvm::errs() << "\n  " << slang::Util::getDateTimeString() << ": FATAL ("                  \
                     << SLANG_FATAL_LEVEL << "):" << __FILE__ << ":" << __func__                   \
                     << "():" << __LINE__ << ":\n"                                                 \
                     << XX << "\n";                                                                        \
    }

namespace slang {
class Util {
    static uint32_t id;

  public:
    /** Get the current date-time string.
     *
     *  Mostly used for logging purposes.
     *
     * @return date-time in "%d-%m-%Y %H:%M:%S" format.
     */
    static std::string getDateTimeString();

    /** Read all contents of the given file.
     *
     * @return contents if successful.
     */
    static std::string readFromFile(std::string fileName);

    /** Append contents to the given fileName.
     *
     * @return zero if failed.
     */
    static int appendToFile(std::string fileName, std::string content);

    /** Write contents to the given fileName.
     *
     * @return zero if failed.
     */
    static int writeToFile(std::string fileName, std::string content);

    static uint32_t getNextUniqueId();
    static std::string getNextUniqueIdStr();

    /** The global level of logging.
     *
     *  Set logging level to SLANG_EVENT_LEVEL on deployment.
     * */
    static uint8_t LogLevel;
};
} // namespace slang

#endif // LLVM_SLANGUTIL_H

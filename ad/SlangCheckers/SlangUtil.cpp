//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SPAN Authors.
//
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
//===----------------------------------------------------------------------===//
// The Util class has general purpose methods.
//===----------------------------------------------------------------------===//

#include "SlangUtil.h"

#include <ctime>
#include <string>
#include <fstream>
#include <sstream>

// TRACE < DEBUG < INFO < EVENT < ERROR < FATAL
uint8_t slang::Util::LogLevel = SLANG_TRACE_LEVEL;
uint32_t slang::Util::id = 0;

std::string slang::Util::getDateTimeString() {
    time_t rawtime;
    struct tm *timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", timeinfo);
    std::string str(buffer);

    return str;
}

// std::string fileName("/home/codeman/.itsoflife/local/tmp/checker-input.txt");
std::string slang::Util::readFromFile(std::string fileName) {
    std::stringstream ss;
    std::ifstream inputTxtFile;
    std::string line;

    inputTxtFile.open(fileName);
    if (inputTxtFile.is_open()) {
        while (std::getline(inputTxtFile, line)) {
            ss << line << "\n";
        }
        inputTxtFile.close();
    } else {
        SLANG_ERROR("Cannot open file '" << fileName);
    }

    return ss.str();
}

int slang::Util::writeToFile(std::string fileName, std::string content) {
    std::ofstream outputTxtFile;

    outputTxtFile.open(fileName);
    if (outputTxtFile.is_open()) {
        outputTxtFile << content;
        outputTxtFile.close();
    } else {
        SLANG_ERROR("SLANG: ERROR: Error writing to file (can't open): '" << fileName);
        return 0;
    }

    return 1;
}

int slang::Util::appendToFile(std::string fileName, std::string content) {
    std::ofstream outputTxtFile;

    outputTxtFile.open(fileName, std::ios_base::app);
    if (outputTxtFile.is_open()) {
        outputTxtFile << content;
        outputTxtFile.close();
    } else {
        SLANG_ERROR("SLANG: ERROR: Error writing to file (can't open): '" << fileName);
        return 0;
    }

    return 1;
}

uint32_t slang::Util::getNextUniqueId() {
    id += 1;
    return id;
}

std::string slang::Util::getNextUniqueIdStr() {
    std::stringstream ss;
    ss << getNextUniqueId();
    return ss.str();
}

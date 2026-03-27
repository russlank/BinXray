// SPDX-License-Identifier: MIT
//
// main.cpp  --  test runner entry point.
//
// Each runXxxTests() function returns true on success.  The process exit
// code reflects the number of failed suites, so CI can gate on it.
//

#include <iostream>

bool runByteFormatterTests();
bool runBinaryDocumentTests();
bool runTransitionMatrixTests();
bool runTransitionSeekerTests();
bool runTrigramPlotTests();

int main() {
    int failedSuites = 0;

    if (!runByteFormatterTests()) {
        ++failedSuites;
    }

    if (!runBinaryDocumentTests()) {
        ++failedSuites;
    }

    if (!runTransitionMatrixTests()) {
        ++failedSuites;
    }

    if (!runTransitionSeekerTests()) {
        ++failedSuites;
    }

    if (!runTrigramPlotTests()) {
        ++failedSuites;
    }

    if (failedSuites == 0) {
        std::cout << "All BinXray tests passed." << std::endl;
        return 0;
    }

    std::cout << failedSuites << " test suite(s) failed." << std::endl;
    return 1;
}

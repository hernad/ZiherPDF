/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A driver for various tests. The idea is that instead of having a separate
   executable and related makefile additions for each test, we have one test
   driver which dispatches desired test based on cmd-line arguments. */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/CmdLineParser.h"
#include "utils/CryptoUtil.h"
#include "utils/DirIter.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/PalmDbReader.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPrettyPrint.h"
#include "mui/Mui.h"
#include "utils/Timer.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"

#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "MobiDoc.h"
#include "HtmlFormatter.h"

// if true, we'll save html content of a mobi ebook as well
// as pretty-printed html to MOBI_SAVE_DIR. The name will be
// ${file}.html and ${file}_pp.html
static bool gSaveHtml = false;
// if true, we'll also save images in mobi files. The name
// will be ${file}_img_${imgNo}.[jpg|png]
// gMobiSaveHtml must be true as well
static bool gSaveImages = false;
// if true, we'll do a layout of mobi files
static bool gLayout = false;
// directory to which we'll save mobi html and images
#define MOBI_SAVE_DIR L"..\\ebooks-converted"

static int Usage() {
    printf("Tester.exe\n");
    printf("  -mobi dirOrFile : run mobi tests in a given directory or for a given file\n");
    printf("  -layout - will also layout mobi files\n");
    printf("  -save-html] - will save html content of mobi file\n");
    printf("  -save-images - will save images extracted from mobi files\n");
    printf("  -zip-create - creates a sample zip file that needs to be manually checked that it worked\n");
    printf("  -bench-md5 - compare Window's md5 vs. our code\n");
    system("pause");
    return 1;
}

/* This benchmarks md5 checksum using fitz code (CalcMD5Digest()) and
Windows' CryptoAPI (CalcMD5DigestWin(). The results are usually in CryptoApi
favor (the first run is on cold cache, the second on warm cache):
10MB
CalcMD5Digest   : 76.913000 ms
CalcMD5DigestWin: 92.389000 ms
diff: -15.476000
5MB
CalcMD5Digest   : 17.556000 ms
CalcMD5DigestWin: 13.125000 ms
diff: 4.431000
1MB
CalcMD5Digest   : 3.329000 ms
CalcMD5DigestWin: 2.834000 ms
diff: 0.495000
10MB
CalcMD5Digest   : 33.682000 ms
CalcMD5DigestWin: 25.918000 ms
diff: 7.764000
5MB
CalcMD5Digest   : 16.174000 ms
CalcMD5DigestWin: 12.853000 ms
diff: 3.321000
1MB
CalcMD5Digest   : 3.534000 ms
CalcMD5DigestWin: 2.605000 ms
diff: 0.929000
*/

static void BenchMD5Size(void* data, size_t dataSize, const char* desc) {
    u8 d1[16], d2[16];
    auto t1 = TimeGet();
    CalcMD5Digest((u8*)data, dataSize, d1);
    double dur1 = TimeSinceInMs(t1);

    auto t2 = TimeGet();
    CalcMD5DigestWin(data, dataSize, d2);
    bool same = memeq(d1, d2, 16);
    CrashAlwaysIf(!same);
    double dur2 = TimeSinceInMs(t2);
    double diff = dur1 - dur2;
    printf("%s\nCalcMD5Digest   : %f ms\nCalcMD5DigestWin: %f ms\ndiff: %f\n", desc, dur1, dur2, diff);
}

static void BenchMD5() {
    size_t dataSize = 10 * 1024 * 1024;
    void* data = malloc(dataSize);
    BenchMD5Size(data, dataSize, "10MB");
    BenchMD5Size(data, dataSize / 2, "5MB");
    BenchMD5Size(data, dataSize / 10, "1MB");
    // repeat to see if timings change drastically
    BenchMD5Size(data, dataSize, "10MB");
    BenchMD5Size(data, dataSize / 2, "5MB");
    BenchMD5Size(data, dataSize / 10, "1MB");
    free(data);
}





// we assume this is called from main sumatradirectory, e.g. as:
// ./obj-dbg/tester.exe, so we use the known files
void ZipCreateTest() {
    const WCHAR* zipFileName = L"tester-tmp.zip";
    file::Delete(zipFileName);
    ZipCreator zc(zipFileName);
    auto ok = zc.AddFile(L"premake5.lua");
    if (!ok) {
        printf("ZipCreateTest(): failed to add makefile.msvc");
        return;
    }
    ok = zc.AddFile(L"premake5.files.lua");
    if (!ok) {
        printf("ZipCreateTest(): failed to add makefile.msvc");
        return;
    }
    ok = zc.Finish();
    if (!ok) {
        printf("ZipCreateTest(): Finish() failed");
    }
}

int TesterMain() {
    RedirectIOToConsole();

    WCHAR* cmdLine = GetCommandLine();

    WStrVec argv;
    ParseCmdLine(cmdLine, argv);

    // InitAllCommonControls();
    // ScopedGdiPlus gdi;
    // mui::Initialize();

    WCHAR* dirOrFile = nullptr;

    bool mobiTest = false;
    size_t i = 2; // skip program name and "/tester"
    while (i < argv.size()) {
        if (str::Eq(argv[i], L"-mobi")) {
            ++i;
            if (i == argv.size()) {
                return Usage();
            }
            mobiTest = true;
            dirOrFile = argv[i];
            ++i;
        } else if (str::Eq(argv[i], L"-layout")) {
            gLayout = true;
            ++i;
        } else if (str::Eq(argv[i], L"-save-html")) {
            gSaveHtml = true;
            ++i;
        } else if (str::Eq(argv[i], L"-save-images")) {
            gSaveImages = true;
            ++i;
        } else if (str::Eq(argv[i], L"-zip-create")) {
            ZipCreateTest();
            ++i;
        } else if (str::Eq(argv[i], L"-bench-md5")) {
            BenchMD5();
            ++i;
        } else {
            // unknown argument
            return Usage();
        }
    }
    if (2 == i) {
        // no arguments
        return Usage();
    }

 

    mui::Destroy();
    system("pause");
    return 0;
}

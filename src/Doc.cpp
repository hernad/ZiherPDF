/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Archive.h"
#include "utils/PalmDbReader.h"
#include "utils/GuessFileType.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "mui/Mui.h"

#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "HtmlFormatter.h"
#include "Doc.h"

Doc::Doc(const Doc& other) {
    Clear();
    type = other.type;
    generic = other.generic;
    error = other.error;
    filePath.SetCopy(other.filePath);
}

Doc& Doc::operator=(const Doc& other) {
    if (this != &other) {
        type = other.type;
        generic = other.generic;
        error = other.error;
        filePath.SetCopy(other.filePath);
    }
    return *this;
}

Doc::~Doc() {
}

// delete underlying object
void Doc::Delete() {
    switch (type) {
        case DocType::None:
            break;
        default:
            CrashIf(true);
            break;
    }

    Clear();
}


void Doc::Clear() {
    type = DocType::None;
    generic = nullptr;
    error = DocError::None;
    filePath.Reset();
}

// the caller should make sure there is a document object
const WCHAR* Doc::GetFilePathFromDoc() const {
    switch (type) {
        case DocType::None:
            return nullptr;
        default:
            CrashIf(true);
            return nullptr;
    }
}

const WCHAR* Doc::GetFilePath() const {
    if (filePath) {
        // verify it's consistent with the path in the doc
        const WCHAR* docPath = GetFilePathFromDoc();
        CrashIf(docPath && !str::Eq(filePath, docPath));
        return filePath;
    }
    CrashIf(!generic && !IsNone());
    return GetFilePathFromDoc();
}

const WCHAR* Doc::GetDefaultFileExt() const {
    switch (type) {
        case DocType::None:
            return nullptr;
        default:
            CrashIf(true);
            return nullptr;
    }
}

WCHAR* Doc::GetProperty(DocumentProperty prop) const {
    switch (type) {
        case DocType::None:
            return nullptr;
        default:
            CrashIf(true);
            return nullptr;
    }
}

std::span<u8> Doc::GetHtmlData() const {
    CrashIf(true);
    return {};
}

ImageData* Doc::GetCoverImage() const {
   
    return nullptr;
}

bool Doc::HasToc() const {
  
    return false;
    
}

bool Doc::ParseToc(EbookTocVisitor* visitor) const {
   
    return false;
   
}

HtmlFormatter* Doc::CreateFormatter(HtmlFormatterArgs* args) const {
    
       CrashIf(true);
       return nullptr;
    
}

Doc Doc::CreateFromFile(const WCHAR* path) {
    Doc doc;
    bool sniff = false;
again:
    Kind kind = GuessFileType(path, sniff);
    
    doc.error = DocError::NotSupported;
    
    if (!sniff) {
        if (doc.IsNone()) {
            sniff = true;
            goto again;
        }
    }

    // if failed to load and more specific error message hasn't been
    // set above, set a generic error message
    if (doc.IsNone()) {
        CrashIf(!((doc.error == DocError::None) || (doc.error == DocError::NotSupported)));
        doc.error = DocError::Unknown;
        doc.filePath.SetCopy(path);
    }
    CrashIf(!doc.generic && !doc.IsNone());
    return doc;
}

bool Doc::IsSupportedFileType(Kind kind) {
    return true;
}

// ==++==
// 
//   
//    Copyright (c) 2006 Microsoft Corporation.  All rights reserved.
//   
//    The use and distribution terms for this software are contained in the file
//    named license.txt, which can be found in the root of this distribution.
//    By using this software in any fashion, you are agreeing to be bound by the
//    terms of this license.
//   
//    You must not remove this notice, or any other, from this software.
//   
// 
// ==--==
/*============================================================
**
** Class:  ISymbolDocument
**
**
** Represents a document referenced by a symbol store. A document is
** defined by a URL and a document type GUID. Using the document type
** GUID and the URL, one can locate the document however it is
** stored. Document source can optionally be stored in the symbol
** store. This interface also provides access to that source if it is
** present.
**
** 
===========================================================*/
namespace System.Diagnostics.SymbolStore {
    
    using System;
	
	// Interface does not need to be marked with the serializable attribute
[System.Runtime.InteropServices.ComVisible(true)]
    public interface ISymbolDocument
    {
        // Properties of the document.
        String URL { get; }
        Guid DocumentType { get; }
    
        // Language of the document.
        Guid Language { get; }
        Guid LanguageVendor { get; }
    
        // Check sum information.
        Guid CheckSumAlgorithmId { get; }
        byte[] GetCheckSum();
    
        // Given a line in this document that may or may not be a sequence
        // point, return the closest line that is a sequence point.
        int FindClosestLine(int line);
        
        // Access to embedded source.
        bool HasEmbeddedSource { get; }
        int SourceLength { get; }
        byte[] GetSourceRange(int startLine, int startColumn,
                                      int endLine, int endColumn);
    }
}

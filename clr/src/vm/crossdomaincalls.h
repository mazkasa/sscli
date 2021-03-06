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
** File:    crossdomaincalls.h
** Purpose: Provides a fast path for cross domain calls
**
===========================================================*/
#ifndef __CROSSDOMAINCALLS_H__
#define __CROSSDOMAINCALLS_H__

#include "frames.h"
#include "methodtable.h"

class   SimpleRWLock;

// These are flags set inside the real proxy. Indicates what kind of type is the proxy cast to
// whether its method table layout is equivalent to the server type etc
#define OPTIMIZATION_FLAG_INITTED               0x01000000
#define OPTIMIZATION_FLAG_PROXY_EQUIVALENT      0x02000000
#define OPTIMIZATION_FLAG_PROXY_SHARED_TYPE     0x04000000
#define OPTIMIZATION_FLAG_DEPTH_MASK            0x00FFFFFF

// This struct has info about methods on MBR objects and Interfaces
struct RemotableMethodInfo
{
    /*
    if XAD_BLITTABLE_ARGS is set, (m_OptFlags & XAD_ARG_COUNT_MASK) contains the number of stack dwords to copy
    */
    enum XADOptimizationType
    { 
        XAD_FLAGS_INITIALIZED   = 0x01000000,
        XAD_NOT_OPTIMIZABLE     = 0x02000000,    // Method call has to go through managed remoting path
        XAD_BLITTABLE_ARGS      = 0x04000000,    // Arguments blittable across domains. Could be scalars or agile gc refs
        XAD_BLITTABLE_RET       = 0x08000000,    // Return Value blittable across domains. Could be scalars or agile gc refs
        XAD_BLITTABLE_ALL       = XAD_BLITTABLE_ARGS | XAD_BLITTABLE_RET,

        XAD_RET_FLOAT           = 0x10000000,
        XAD_RET_DOUBLE          = 0x20000000,
        XAD_RET_HFA_TYPE        = 0x40000000,
        XAD_RET_GC_REF          = 0x70000000,    // To differentiate agile objects like string which can be blitted across domains, but are gc refs
        XAD_RET_FLOAT_MASK      = 0x30000000,
        XAD_RET_TYPE_MASK       = 0x70000000,

        XAD_METHOD_IS_VIRTUAL   = 0x80000000,
        XAD_ARGS_HAVE_A_FLOAT   = 0x00800000,

        XAD_FLAG_MASK           = 0xFF800000,
        XAD_ARG_COUNT_MASK      = 0x007FFFFF
    } ;

    static XADOptimizationType IsCrossAppDomainOptimizable(MethodDesc *pMeth, DWORD *pNumDwordsToCopy);

    static DWORD GetSizeOfRemotableMethodInfo(MethodTable *pMT);

    static BOOL TypeIsConduciveToBlitting(MethodTable *pFromMT, MethodTable *pToMT);
        
    static BOOL AreArgsBlittable(XADOptimizationType enumVal)
    {
        LEAF_CONTRACT;
        return (enumVal & XAD_BLITTABLE_ARGS) && IsReturnBlittable(enumVal);
    }
    static BOOL IsReturnBlittable(XADOptimizationType enumVal)
    {
        LEAF_CONTRACT;
        return enumVal & XAD_BLITTABLE_RET;
    }
    static BOOL IsReturnGCRef(XADOptimizationType enumVal)
    {
        LEAF_CONTRACT;
        return XAD_RET_GC_REF == (enumVal & XAD_RET_TYPE_MASK);
    }

    static UINT GetFPReturnSize(XADOptimizationType enumVal)
    {
        WRAPPER_CONTRACT;
        if (IsReturnGCRef(enumVal))
            return 0;
        else
        {
            if (enumVal & XAD_RET_HFA_TYPE)
            {
                return ( ((enumVal & XAD_RET_FLOAT_MASK) >> 28) + (ELEMENT_TYPE_R4_HFA - 1) );
            }
            else
            {
                return ((enumVal & XAD_RET_TYPE_MASK) >> 26);
            }
        }
    }

    static DWORD GetRetTypeFlagsFromFPReturnSize(UINT fpRetSize)
    {
        LEAF_CONTRACT;

        //  fpRetSize   flags
        //  0x0           0
        //  0x4           0x100000000
        //  0x8           0x200000000
        //  0x46          0x400000000 + 0x10000000  (ELEMENT_TYPE_R4_HFA == 0x46)
        //  0x47          0x400000000 + 0x20000000  (ELEMENT_TYPE_R8_HFA == 0x47)
        C_ASSERT(XAD_RET_FLOAT  == 4 << 26);
        C_ASSERT(XAD_RET_DOUBLE == 8 << 26);
        C_ASSERT(ELEMENT_TYPE_MODIFIER == 0x40);
        C_ASSERT(ELEMENT_TYPE_R4_HFA   == 0x46);
        C_ASSERT(ELEMENT_TYPE_R8_HFA   == 0x47);

        DWORD flags;
        if (fpRetSize & ELEMENT_TYPE_MODIFIER)
        {
            flags = ((fpRetSize - (ELEMENT_TYPE_R4_HFA - 1)) << 28);
            flags |= XAD_RET_HFA_TYPE;
        }
        else
        {
            flags = (fpRetSize << 26);
        }

        _ASSERTE(fpRetSize == GetFPReturnSize((XADOptimizationType)flags));

        return flags;
    }

    static BOOL DoArgsContainAFloat(XADOptimizationType enumVal)
    {
        LEAF_CONTRACT;
        return enumVal & XAD_ARGS_HAVE_A_FLOAT;
    }

    static BOOL IsCallNotOptimizable(XADOptimizationType enumVal)
    {
        LEAF_CONTRACT;
        return enumVal & XAD_NOT_OPTIMIZABLE;
    }
    static BOOL IsMethodVirtual(XADOptimizationType enumVal)
    {
        LEAF_CONTRACT;
        return enumVal & XAD_METHOD_IS_VIRTUAL;
    }

    private:

    static DWORD DoStaticAnalysis(MethodDesc *pMeth);
    
    DWORD       m_OptFlags;
    
} ;

class CrossDomainOptimizationInfo
{
    RemotableMethodInfo m_rmi[0];

    public:

    RemotableMethodInfo *GetRemotableMethodInfo()
    {
        return &(m_rmi[0]);
    }
};

class CrossDomainTypeMap
{
    class MTMapEntry
    {
        public:
            MTMapEntry(AppDomain *pFromDomain, MethodTable *pFromMT, AppDomain *pToDomain, MethodTable *pToMT);
            UPTR GetHash()
            {
                LEAF_CONTRACT;
                DWORD hash = _rotl((UINT)(SIZE_T)m_pFromMT, 1) + m_dwFromDomain.m_dwId;
                hash = _rotl(hash, 1) + m_dwToDomain.m_dwId;
                return (UPTR)hash;
            }
            ADID        m_dwFromDomain;
            ADID        m_dwToDomain;
            MethodTable *m_pFromMT;
            MethodTable *m_pToMT;
    };

    static BOOL                 CompareMTMapEntry (UPTR val1, UPTR val2);
    static PtrHashMap           *s_crossDomainMTMap; // Maps a MT to corresponding MT in another domain
    static SimpleRWLock         *s_MTMapLock;
    static PtrHashMap *         GetTypeMap();

public:
    static MethodTable *GetMethodTableForDomain(MethodTable *pFrom, AppDomain *pFromDomain, AppDomain *pToDomain);
    static void SetMethodTableForDomain(MethodTable *pFromMT, AppDomain *pFromDomain, MethodTable *pToMT, AppDomain *pToDomain);       
    static void FlushStaleEntries();
};

#if defined(_X86_) || defined(_WIN64)

ARG_SLOT DispatchCall(
                    SIZE_T *pSrc,
                    DWORD numStackSlotsToCopy, 
                    SIZE_T *pRegisterArgs,
                    UINT64 uRegTypeMap,
                    LPVOID pvRetBuff,
                    UINT64 cbRetBuff,
                    UINT32 fpRetSize,
                    BYTE *pTargetAddress,
                    OBJECTREF *pRefException,
                    ContextTransitionFrame* pFrame = NULL);

ARG_SLOT DispatchCallNoEH(
                    SIZE_T *pSrc,
                    DWORD numStackSlotsToCopy, 
                    SIZE_T *pRegisterArgs,
                    UINT64 uRegTypeMap,
                    LPVOID pvRetBuff,
                    UINT64 cbRetBuff,
                    UINT32 fpRetSize,
                    BYTE *pTargetAddress);

struct MarshalAndCallArgs;
void MarshalAndCall_Wrapper2(MarshalAndCallArgs * pArgs);

class CrossDomainChannel
{
private:
    friend void MarshalAndCall_Wrapper2(MarshalAndCallArgs * pArgs);
    

    BOOL GetTargetAddressFast(DWORD optFlags, MethodTable *pSrvMT, BOOL bFindServerMD);
    BOOL GetGenericMethodAddress(MethodTable *pSrvMT);
    BOOL GetInterfaceMethodAddressFast(DWORD optFlags, MethodTable *pSrvMT, BOOL bFindServerMD);
    BOOL BlitAndCall(ARG_SLOT *pReturn);
    BOOL MarshalAndCall(ARG_SLOT *pReturn);
    void MarshalAndCall_Wrapper(MarshalAndCallArgs * pArgs);
    BOOL ExecuteCrossDomainCall(ARG_SLOT *pReturn);
    VOID RenewLease();
    OBJECTREF GetServerObject();
    BOOL InitServerInfo();
    OBJECTREF ReadPrincipal();
    VOID RestorePrincipal(OBJECTREF *prefPrincipal);
    VOID ResetPrincipal();

public:

    UINT GetFPReturnSize()
    {
        WRAPPER_CONTRACT;
        return RemotableMethodInfo::GetFPReturnSize(m_xret);
    }
    
    BOOL CheckCrossDomainCall(TPMethodFrame *pFrame, ARG_SLOT *pReturn);

private:
    MethodDesc                          *m_pCliMD;
    MethodDesc                          *m_pSrvMD;
    RemotableMethodInfo::XADOptimizationType    m_xret;
    DWORD                               m_numStackSlotsToCopy;
    OBJECTHANDLE                        m_refSrvIdentity;
    AppDomain                           *m_pCliDomain;
    ADID                                m_pSrvDomain;
    BYTE                                *m_pTargetAddress;
    TPMethodFrame                       *m_pFrame;
};

#endif
#endif


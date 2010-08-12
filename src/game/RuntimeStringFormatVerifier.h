/*
 * Copyright (C) 2005-2010 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __RUNTIMESTRINGVERIFIER_H
#define __RUNTIMESTRINGVERIFIER_H

#include <typeinfo>

#include "Language.h"
#include "ObjectMgr.h"

namespace StringVerify
{
    static const char FLAG_CHARS[]={'-', '+', ' ', '#', 0};
    bool isFlag(char c)
    {
        const char* chars = FLAG_CHARS;
        while(*chars)
        {
            if(*chars==c)
                return true;
            ++chars;
        }
        return false;
    }

    ///
    /// moves a pointer in a format string towards the
    /// next type. Based upon the ANSI-C specs from
    /// http://flash-gordon.me.uk/ansi.c.txt
    ///
    char getNextType(const char*& format)
    {
        format =  strchr(format, '%');
        if(!format)
            return 0;

        ++format;

        // flags
        while(*format && isFlag(*format))
            ++format;

        // width
        while(*format && isdigit(*format))
            ++format;

        // precision
        while(*format && (*format=='.' || isdigit(*format)))
            ++format;

        if(*format=='%')
            return getNextType(format);
        return *format;
    }

    void printUsedTypes(const char* format)
    {
        while(format && *format)
        {
            getNextType(format);
            if(!format || !*format)
                break;
            printf("%%%c ", *(format));
        }
    }

    template <class ArgList>
    void printExpectedParameter()
    {
        printf("%s ", typeid(typename ArgList::Head).name());
        printExpectedParameter<typename ArgList::Tail>();
    }

    template <class ArgList>
    bool internalVerify(const char* format)
    {
        // non existing mangos strings
        if(!format)
        {
            return typeid(typename ArgList::Tail) == typeid(EndOfList);
        }

        getNextType(format);
        if(format && *format)
        {
            switch(*format)
            {
                case 's':
                {
                    if(typeid(typename ArgList::Head)!= typeid(const char*) &&
                       typeid(typename ArgList::Head)!= typeid(char*))
                    {
                        return false;
                    }
                    break;
                }
                case 'd':
                case 'i':
                case 'o':
                case 'u':
                case 'x':
                case 'X':
                {
                    if(typeid(typename ArgList::Head)!= typeid(int))
                    {
                        return false;
                    }
                    break;
                }
                
                case 'f':
                case 'F':
                case 'e':
                case 'E':
                case 'g':
                case 'G':
                {
                    if(typeid(typename ArgList::Head)!= typeid(double))
                    {
                        return false;
                    }
                    break;
                }
                case 'c':
                {
                    if(typeid(typename ArgList::Head)!= typeid(char))
                    {
                        return false;
                    }
                    break;
                }
                default:
                    printf("RuntimeVerifier: unknown type char '%c'\n", *format);
                    return false;
            }
            return internalVerify<typename ArgList::Tail>(format);
        }

        return typeid(typename ArgList::Tail) == typeid(EndOfList);
    }

    template<>
    void printExpectedParameter<EndOfList>()
    {
    }

    template<>
    bool internalVerify<EndOfList>(const char* format)
    {
        if(format)
            return getNextType(format) == 0;
        // empty param list with empty format - ok
        return true;
    }
};


///
/// Redirects specializations which exceed the maximum to 0.
/// Due to the specialization of this case, this ends
/// the verifyAll recursion.
//
template<int64 IN>
struct IntLimiter
{
    enum {Result = IN>LANG_MAX_USED_MANGOS_ID?IN:0};
};

using namespace StringVerify;

template<int64 IDX, int64 DEPTH, int64 MAXNUM_IN_DEPTH>
class RuntimeVerifier
{
    private:
    static const char FLAG_CHARS[];

    public:
    ///
    /// This is the template version of a simple for-loop.
    ///
    /// "ANSI/ISO C++ conforming programs must not rely on a maximum [template] depth greater than 17"
    /// Therefore we make 8 calls in each level. This allows us to make "\sum_{i=0}^{17-1} 8^i"
    /// calls at all - think about it as a tree. This is enough to check all 2000000000 mangos
    /// reserved strings. We took 8 because this allows us to do static potency calculations
    /// using the shift operator.
    ///
    static bool verifyAll()
    {
        for(int i=0; i< MAX_LOCALE; ++i)
        {
            const char* str = sObjectMgr.GetMangosString((int32)IDX, i);
            if(!str)
               continue;
            if(!internalVerify<typename VerifiableIndex<IDX>::Parameter>(str))
            {
                printf("Failed to verify mangos_string entry #%llu. Expected parameters are '", IDX);
                printExpectedParameter<typename VerifiableIndex<IDX>::Parameter>();
                printf("' but used types are '");
                printUsedTypes(str);
                printf("'\n");
                return false;
            }
            printf("verified mangos string #%llu\n", IDX);
        }

        // 8^x = (2^3)^x = 2^(3*x) = 1 << (3*x)

        // for first row starting at 1: 1
        // OK, range 1..8
        const int64 indexInThisDepth = IDX - (MAXNUM_IN_DEPTH -(1<<(3*(DEPTH))));


        const int64 firstNextCallIndex = MAXNUM_IN_DEPTH +1+8*(indexInThisDepth-1);

        const int64 maxInNextDepth = MAXNUM_IN_DEPTH + 1<<(3*(DEPTH+1));

        return RuntimeVerifier<IntLimiter<firstNextCallIndex>::Result, DEPTH+1, maxInNextDepth>::verifyAll() &&
            RuntimeVerifier<IntLimiter<firstNextCallIndex+1>::Result, DEPTH+1, maxInNextDepth>::verifyAll() &&
            RuntimeVerifier<IntLimiter<firstNextCallIndex+2>::Result, DEPTH+1, maxInNextDepth>::verifyAll() &&
            RuntimeVerifier<IntLimiter<firstNextCallIndex+3>::Result, DEPTH+1, maxInNextDepth>::verifyAll() &&
            RuntimeVerifier<IntLimiter<firstNextCallIndex+4>::Result, DEPTH+1, maxInNextDepth>::verifyAll() &&
            RuntimeVerifier<IntLimiter<firstNextCallIndex+5>::Result, DEPTH+1, maxInNextDepth>::verifyAll() &&
            RuntimeVerifier<IntLimiter<firstNextCallIndex+6>::Result, DEPTH+1, maxInNextDepth>::verifyAll() &&
            RuntimeVerifier<IntLimiter<firstNextCallIndex+7>::Result, DEPTH+1, maxInNextDepth>::verifyAll();

    }
};

template<int64 DEPTH, int64 MAXNUM_IN_DEPTH>
class RuntimeVerifier<0, DEPTH, MAXNUM_IN_DEPTH>
{
    public:
    static bool verifyAll()
    {
        return true;
    }
};

#endif

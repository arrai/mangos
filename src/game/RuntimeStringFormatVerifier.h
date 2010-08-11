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

struct RuntimeVerifier
{
    private:
    static const char FLAG_CHARS[];

    static bool isCharType(const char* chars, char c)
    {
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
    static char getNextType(const char*& format)
    {
        format =  strchr(format, '%');
        if(!format)
            return 0;

        ++format;

        // flags
        while(*format && isCharType(FLAG_CHARS, *format))
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

    static void printUsedTypes(const char* format)
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
    static void printExpectedParameter()
    {
        printf("%s ", typeid(typename ArgList::Head).name());
        printExpectedParameter<typename ArgList::Tail>();
    }

    template <class ArgList>
    static bool internalVerify(const char* format)
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

    public:
    ///
    /// This is the template version of a simple for-loop.
    /// "ANSI/ISO C++ conforming programs must not rely on a maximum [template] depth greater than 17"
    /// Therefore we make 5 calls in each level. This allows us to make "\sum_{i=0}^{17-1} 1 * 5^i"
    /// calls at all. This is enough to check all 2000000000 mangos reserved strings.
    ///
    template<int IDX>
    static bool verifyAll()
    {
        for(int i=0; i< MAX_LOCALE; ++i)
        {
            const char* str = sObjectMgr.GetMangosString(IDX, i);
            if(str && !internalVerify<typename VerifiableIndex<IDX>::Parameter>(str))
            {
                printf("Failed to verify mangos_string entry #%u. Expected parameters are '", IDX);
                printExpectedParameter<typename VerifiableIndex<IDX>::Parameter>();
                printf("' but used types are '");
                printUsedTypes(str);
                printf("'\n");
                return false;
            }
        }
        return RuntimeVerifier::verifyAll<IDX-1>();
    }
};

const char RuntimeVerifier::FLAG_CHARS[] = {'-', '+', ' ', '#', 0};

template<>
void RuntimeVerifier::printExpectedParameter<EndOfList>()
{
}

template<>
bool RuntimeVerifier::internalVerify<EndOfList>(const char* format)
{
    if(format)
        return getNextType(format) == 0;
    // empty param list with empty format - ok
    return true;
}

// verifier recursion termination
template<>
bool RuntimeVerifier::verifyAll<0>()
{
    return true;
}


#endif

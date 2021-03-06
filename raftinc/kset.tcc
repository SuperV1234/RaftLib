/**
 * kset.tcc - class to contain a set of kernels, this one
 * only exists temporarily in order to make construction
 * of complex graphs relatively simple.
 *
 * @author: Jonathan Beard
 * @version: Wed Apr 13 12:50:38 2016
 *
 * Copyright 2016 Jonathan Beard
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _KSET_TCC_
#define _KSET_TCC_  1
#include <type_traits>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <cassert>
#include <functional>



namespace raft
{

/** predeclare raft::kernel **/
class kernel;

/**
 * dummy class for extending to kset, largely used
 * to keep from having to type out the class type
 * in long form within the kpair/map classes.
 */
struct basekset{
    constexpr basekset() = delete;
    ~basekset() = delete;

    using iterator_t = typename
        std::vector< std::reference_wrapper< raft::kernel > >::iterator;

    virtual iterator_t begin()      = 0;
    virtual iterator_t end()        = 0;
};

/** pre-declaration **/
template < class... KERNELS > struct AddKernel;

/**
 * feel hacky doing this, but it's probably the
 * least amount of code I can write to get this
 * done....probably better way using a tuple
 * like construct, then the compiler would do
 * most of the work
 */

template < class... K > class ksetr
#ifndef TEST_WO_RAFT
: public basekset
#endif
{
private:
    static_assert( sizeof...( K ) > 0, "size for kset must be > 0" );
    using common_t = typename std::common_type< K... >::type;
#ifndef TEST_WO_RAFT
    /**
     * interestingly this class is far more generic, but for RaftLib
     * we need the common_t to be either raft::kernel, or a dervied
     * type of raft::kernel
     */
    static_assert( std::is_base_of< basekset,
                                    common_t >::value,
                   "The common type of ksetr must be a derived class of raft::kernel" );
#endif
    /** don't want to type this over and over **/
    using vector_t = std::vector< std::reference_wrapper< common_t > >;
    vector_t  k;

public:
    /**
     * base constructor, with multiple args.
     */
    constexpr
    ksetr( K&... kernels )
    {
        /**
         * until we get the tuple-like solution built, dynamic alloc
         * will work, just use reserve to keep from allocating for
         * every subsequent call
         */
        k.reserve( sizeof...( K ) );
        AddKernel< K... >::add( std::forward< K >( kernels )..., k );
    }

    /**
     * move constructor, hopefully keep the dynamic mem
     * in vector valid w/o re-allocating and copying.
     */
    constexpr
    ksetr( ksetr< K... > &&other ) : k( std::move( other.k ) ){}

    /**
     * a bit hacky, but wanted to use iterators, seems
     * like this is the siplest way since the compiler
     * will only check for begin and end.
     */
    virtual decltype( k.cbegin() ) begin()
    {
        return( k.cbegin() );
    }

    /**
     * returns end iterator
     */
    virtual decltype( k.cend() ) end()
    {
        return( k.cend() );
    }

    /**
     * default destructor, nothing really to destruct
     */
    virtual ~ksetr() = default;

    /**
     * get the number of kernels held.
     */
    constexpr typename vector_t::size_type size() const { return( sizeof...( K ) ); }

    /**
     * get the common, base class shared between
     * the kernels, probably raft::kernel but maybe
     * not.
     */
    using value_type = common_t;
};


/**
 * resursive AddKernel, keep going
 */
template < class K, class... KS > struct AddKernel< K, KS... >
{
    template < class CONTAINER >
        static void add( K &&kernel,
                         KS&&... kernels,
                         CONTAINER &c )
    {
        c.emplace_back( kernel );
        AddKernel< KS... >::add( std::forward< KS >( kernels )..., c );
        return;
    }
};

/**
 * base case for recursive AddKernel, stop
 * the recursion.
 */
template < class K > struct AddKernel< K >
{
    template < class CONTAINER >
        static void add( K &&kernel, CONTAINER &c )
    {
        c.emplace_back( kernel );
        return;
    }
};

/**
 * wrap so I can get away w/o having to do the class vs. constructor
 * template args
 */
template< class... K >
inline
static
ksetr< K... >
kset( K&&... kernels )
{
    return( ksetr< K... >( std::forward< K >( kernels )... ) );
}

} /** end namespace raft */
#endif /* END _KSET_TCC_ */

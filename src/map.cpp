/**
 * map.cpp -
 * @author: Jonathan Beard
 * @version: Fri Sep 12 10:28:33 2014
 *
 * Copyright 2014 Jonathan Beard
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
#include <sstream>
#include <map>
#include <sstream>
#include <vector>
#include <typeinfo>
#include <cstdio>
#include <cassert>
#include <string>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <array>
#include <typeinfo>
#include "common.hpp"
#include "map.hpp"
#include "graphtools.hpp"
#include "kpair.hpp"
#include "mapexception.hpp"

raft::map::map() : MapBase()
{

}

void
raft::map::checkEdges( kernelkeeper &source_k )
{
   auto &container( source_k.acquire() );
   /**
    * NOTE: will throw an error that we're not catching here
    * if there are unconnected edges...this is something that
    * a user will have to fix.  Otherwise will return with no
    * errors.
    */
   GraphTools::BFS( container,
                    []( PortInfo &a, PortInfo &b, void *data )
                    {
                       (void) a;
                       (void) b;
                       (void) data;
                       return;
                    },
                    nullptr,
                    true );
   source_k.release();
   return;
}

/**
   void insert( raft::kernel &a,  PortInfo &a_out,
                raft::kernel &b,  PortInfo &b_in,
                raft::kernel &i,  PortInfo &i_in, PortInfo &i_out );
**/
void
raft::map::enableDuplication( kernelkeeper &source, kernelkeeper &all )
{
    auto &source_k( source.acquire() );
    auto &all_k   ( all.acquire()    );
    /** don't have to do this but it makes it far more apparent where it comes from **/
    void * const kernel_ptr( reinterpret_cast< void* >( &all_k ) );
    using kernel_ptr_t =
      typename std::remove_reference< decltype( all_k ) >::type;
    /** need to grab impl of Lengauer and Tarjan dominators, use for SESE **/
    /** in the interim, restrict to kernels that are simple to duplicate **/
    GraphTools::BFS( source_k,
                     []( PortInfo &a, PortInfo &b, void *data )
                     {
                        auto * const all_k( reinterpret_cast< kernel_ptr_t* >( data ) );
                        if( a.out_of_order && b.out_of_order )
                        {
                           /** case of inline kernel **/
                           if( b.my_kernel->input.count() == 1 &&
                               b.my_kernel->output.count() == 1 &&
                               a.my_kernel->dup_candidate  )
                           {
                              auto *kernel_a( a.my_kernel );
                              assert( kernel_a->input.count() == 1 );
                              auto &port_info_front( kernel_a->input.getPortInfo() );
                              auto *front( port_info_front.other_kernel );
                              auto &front_port_info( front->output.getPortInfo() );
                              /**
                               * front -> kernel_a goes to
                               * front -> split -> kernel_a
                               */
                              auto *split(
                                 static_cast< raft::kernel* >(
                                    port_info_front.split_func() ) );
                              all_k->insert( split );
                              MapBase::insert( front,    front_port_info,
                                               kernel_a, port_info_front,
                                               split );

                              assert( kernel_a->output.count() == 1 );

                              /**
                               * now we need the port info from the input
                               * port on back
                               **/

                              /**
                               * kernel_a -> back goes to
                               * kernel_a -> join -> back
                               */
                              auto *join( static_cast< raft::kernel* >( a.join_func() ) );
                              all_k->insert( join );
                              MapBase::insert( a.my_kernel, a,
                                               b.my_kernel, b,
                                               join );
                              /**
                               * finally set the flag to the scheduler
                               * so that the parallel map manager can
                               * pick it up an use it.
                               */
                              a.my_kernel->dup_enabled = true;
                           }
                           /** parallalizable source, single output no inputs**/
                           else if( a.my_kernel->input.count() == 0 &&
                                    a.my_kernel->output.count() == 1 )
                           {
                              auto *join( static_cast< raft::kernel* >( a.join_func() ) );
                              all_k->insert( join );
                              MapBase::insert( a.my_kernel, a,
                                               b.my_kernel, b,
                                               join );
                              a.my_kernel->dup_enabled = true;
                           }
                           /** parallelizable sink, single input, no outputs **/
                           else if( b.my_kernel->input.count() == 1 &&
                                    b.my_kernel->output.count() == 0 )
                           {
                              auto *split(
                                 static_cast< raft::kernel* >( b.split_func() ) );
                              all_k->insert( split );
                              MapBase::insert( a.my_kernel, a,
                                               b.my_kernel, b,
                                               split );
                              b.my_kernel->dup_enabled = true;
                           }
                           /**
                            * flag as candidate if the connecting
                            * kernel only has one input port.
                            */
                           else if( b.my_kernel->input.count() == 1 )
                           {
                              /** simply flag as a candidate **/
                              b.my_kernel->dup_candidate = true;
                           }

                        }
                     },
                     kernel_ptr,
                     false );
   source.release();
   all.release();
}

void
raft::map::joink( kpair * const next )
{
        /** might be able to do better by re-doing with templates **/
        if( next->has_src_name && next->has_dst_name )
        {
            if( next->out_of_order )
            {
                (this)->link< raft::order::out >( next->src,
                                                  next->src_name,
                                                  next->dst,
                                                  next->dst_name );
            }
            else
            {
                (this)->link( next->src,
                              next->src_name,
                              next->dst,
                              next->dst_name );

            }
        }
        else if( next->has_src_name && ! next->has_dst_name )
        {
            if( next->out_of_order )
            {
                (this)->link< raft::order::out >( next->src,
                                                  next->src_name,
                                                  next->dst );
            }
            else
            {
                (this)->link( next->src,
                              next->src_name,
                              next->dst );
            }
        }
        else if( ! next->has_src_name && next->has_dst_name )
        {
            if( next->out_of_order )
            {
                (this)->link< raft::order::out >( next->src,
                                                  next->dst,
                                                  next->dst_name );
            }
            else
            {
                (this)->link( next->src,
                              next->dst,
                              next->dst_name );
            }
        }
        else /** single input, single output, hopefully **/
        {
            if( next->out_of_order )
            {
                (this)->link< raft::order::out >( next->src,
                                                  next->dst );
            }
            else
            {
                (this)->link( next->src,
                              next->dst );
            }
        }
}



kernel_pair_t
raft::map::operator += ( kpair &p )
{
    kpair * const pair = &p;
    assert( pair != nullptr );
    raft::kernel *end( pair->dst );
    /** start at the head, go forward **/
    kpair *next = pair->head;
    assert( next != nullptr );
    raft::kernel *start( next->src );

    /** used to do nested splits **/
    enum sj_t : int8_t { split, join, cont };
    const static std::array< std::string, 3 > sj_t_str = { "split", "join", "cont" };

    auto next_link_type( []( const kpair * const k ) -> sj_t
    {
        if( k->split_to && ! k->join_from )
        {
            return( split );
        }
        else if( ! k->split_to && k->join_from )
        {
            return( join );
        }
        else if( ! k->split_to && ! k->join_from )
        {
            return( cont );
        }
        else
        {
            throw InvalidTopologyOperationException( "Invalid ordering of topology operators \"<=\", \">>\", \">=.\" Please check and try again\n" );
        }
        //should never reach here **/
        assert( false );
        return( cont );
    });

    std::stack< sj_t > stack;
    /**
     * dup kernels arranged in "groups" so that
     * a =< b
     * a -> b1
     * a -> b2
     * and b^n is one group
     * each kernel attached to b^n is
     * then chained group to group
     * until we reduce
     */
    kernels_t  groups;

    /**
     * keep # of out-edges for splits
     */
    split_stack_t split_out_edges;

    /** could throw an exception so use push_back **/
    groups.push_back( up_group_t( new group_t( ) ) );
    /**
     * start kernels vector off
     * emplace used since kernel can't throw an exception
     */
    groups.back()->emplace_back( next->src );

    /** hold on to these for a bit so we can back track if needed **/
    std::vector< kpair* > kpair_store;

    /** loop over chain **/
    while( next != nullptr )
    {
        stack.push( next_link_type( next ) );
        decltype( groups ) temp_groups;
        switch( stack.top() )
        {

            case( cont ):
            {
                inline_cont( groups,
                             temp_groups,
                             next );
                //done, move on
                stack.pop();
            }
            break;
            case( split ):
            {
                inline_split( split_out_edges,
                              groups,
                              temp_groups,
                              next );
            }
            break;
            case( join ):
            {
                //make sure something bad didn't happen
                stack.pop();
                if( stack.size() == 0  /** mult kernels going ot single kernel **/ )
                {
                    inline_dup_join( groups,
                                     temp_groups,
                                     next );
                }
                else if( stack.top() == split )
                {
                    //can pop now, don't need the old split
                    stack.pop();
                    inline_join( split_out_edges,
                                 groups,
                                 temp_groups,
                                 next );
                }
                else
                {
                    assert( false );
                }
            }
            break;
            default:
                assert( false );
        }

        groups.clear();
        groups = std::move( temp_groups );


        kpair *temp_kpair = next;
        next = next->next;
        kpair_store.emplace_back( temp_kpair );
    }
    /**
     * we should be emptying each time, so this should
     * always be zero or one at most
     */
    assert( groups.size() < 2 );
    for( auto *ptr : kpair_store )
    {
        delete( ptr );
    }
    /**
     * FIXME, re-write kernel_pair_t so it returns struct with
     * iterators so that we can access an array of ends if the
     * run-time creates them.
     */
    return( kernel_pair_t( start, end ) );
}

void
raft::map::inline_cont( kernels_t &groups,
                        kernels_t &temp_groups,
                        kpair * const next )
{
    /**
     * for each kernel, and each group make an output
     * link inline
     */
    for( auto &group : groups )
    {
        /** we need to insert a new group in temp_groups **/
        temp_groups.push_back( up_group_t( new group_t() ) );
        for( auto *kernel : *group )
        {
            assert( kernel != nullptr );
            if( temp_groups[ 0 ]->size() == 0 )
            {
                //simply join source do destination
                joink( next );
                //emplace back temp_groups kernel
                temp_groups[ 0 ]->emplace_back( next->dst );
            }
            else
            {
                auto * const dst_clone( next->dst->clone() );
                next->src = kernel;
                next->dst = dst_clone;
                joink( next );
                temp_groups.back()->emplace_back( dst_clone );
            }
        }
    }

    return;
}

void
raft::map::inline_split( split_stack_t &split_stack,
                         kernels_t     &groups,
                         kernels_t     &temp_groups,
                         kpair * const next )
{
    split_stack.push( next->src_out_count );
    /**
     * for each kernel, create a destination
     * for each of it's output ports, link it
     */
    for( auto &group : groups )
    {
        /** each group will have the same # of out-edges, save **/
        for( auto *kernel : *group )
        {
            //TODO: throw an exception for
            //only one port, user meant
            //to use raft::out likely not
            //the static split

            //TODO: throw exception for more
            //than one input port for destination
            //won't work with split/join
            temp_groups.push_back( up_group_t( new group_t() ) );
            //for each output port in kernel
            if( temp_groups[ 0 ]->size() == 0 )
            {
                auto &port( kernel->output );
                for( auto it( port.begin() );
                        it != port.end(); ++it )
                {
                    next->src = kernel;
                    //next->dst already eq dst
                    next->src_name = it.name();
                    next->has_src_name = true;
                    if( it == port.begin() )
                    {
                        temp_groups.back()->emplace_back( next->dst );
                    }
                    else
                    {
                        auto * const dst_clone( next->dst->clone() );
                        next->dst = dst_clone;
                        //dst name should be same as first
                        temp_groups.back()->emplace_back( dst_clone );
                    }
                    joink( next );
                }
            }
            else
            {
                auto &port( kernel->output );
                for( auto it( port.begin() );
                        it != port.end(); ++it )
                {
                    next->src = kernel;
                    //next->dst already eq dst
                    next->src_name = it.name();
                    next->has_src_name = true;
                    //now we always need to clone
                    auto * const dst_clone( next->dst->clone() );
                    next->dst = dst_clone;
                    //dst name should be same as first
                    temp_groups.back()->emplace_back( dst_clone );
                    joink( next );
                }
            }
        }
    }
    return;
}

void
raft::map::inline_join( split_stack_t &split_stack,
                        kernels_t     &groups,
                        kernels_t     &temp_groups,
                        kpair * const next )
{
    /** need to get the count **/
    auto count( 0 );
    /**
     * for every kernel in each group, join that
     * group's kernels to a single destination and
     * "join"
     */
    auto joinfunc( [&]( up_group_t &group, kpair *n )
    {
        auto it( group->begin() );
        for( auto port_it( n->dst->input.begin() );
                port_it != n->dst->input.end(); ++port_it )
        {
            n->src      = (*it);
            n->dst_name = port_it.name();
            n->has_dst_name = true;
            joink( n );
            ++it;
        }

    });

    for( auto &group : groups )
    {
        /**
         * need to add exception for too many
         * output ports for source, or too
         * few input ports for destination
         */

        if( count <= 0 )
        {
            temp_groups.push_back( up_group_t( new group_t() ) );
            count = split_stack.top();
        }
        if( temp_groups[ 0 ]->size() == 0 )
        {
            /** use first kernel **/
            temp_groups.back()->emplace_back( next->dst );
            /**
             * take all kernels from first group, attach
             * to current dst. kernel
             */
            joinfunc( group, next );
        }
        else
        {
            /**
             * we need to clone a kernel
             */
            auto *temp_groups_k( next->dst->clone() );
            next->dst = temp_groups_k;
            temp_groups.back()->emplace_back( temp_groups_k );
            joinfunc( group, next );
        }
        count--;
    }
    split_stack.pop();
}


void
raft::map::inline_dup_join( kernels_t &groups,
                            kernels_t &temp_groups,
                            kpair * const next )
{
    //only one group
    assert( groups.size() == 1 );
    temp_groups.push_back( up_group_t( new group_t() ) );
    up_group_t &gp( groups[ 0 ] );
    const auto dest_in_count( next->dst_in_count );
    (void) gp;
    (void) dest_in_count;


    for( auto port_it( next->dst->input.begin() );
        port_it != next->dst->input.end(); ++port_it )
    {
        if( port_it == next->dst->input.begin() )
        {
            /** connect source to dest **/
            next->dst_name = port_it.name();
            next->has_dst_name = true;
            joink( next );
            temp_groups.back()->emplace_back( next->dst );
            /**
             * at this point a single chain has been added
             * but we need to check for the next one to make
             * sure that we don't need to duplicate from head
             */
        }
        else
        {
            /**
             * need to find out if there's more than one kernel in front
             * of this one. don't need to keep track of the new kernels
             * here.
             */
            raft::kernel *src( nullptr );
            if( next->head != next )
            {
                /** go back to head **/
                auto *head_next( next->head );
                while( head_next != next )
                {
                    if( src == nullptr )
                    {
                        src = head_next->src->clone();
                    }
                    auto *dst( head_next->dst->clone() );
                    head_next->src = src;
                    head_next->dst = dst;
                    joink( head_next );
                    src = dst;
                    head_next = next;
                }
                /** now need to link chain to current join **/
                //src already alloced
                assert( src != nullptr );
                next->dst_name = port_it.name();
                next->has_dst_name = true;
                next->src = src;
                joink( next );
            }
            else /** only one kernel **/
            {
                //need to duplicate source
                next->src = next->src->clone();
                next->dst_name = port_it.name();
                next->has_dst_name = true;
                joink( next );
            }
        }
    }
    return;
}

/*
   Copyright (c) 2009-2011, Jack Poulson
   All rights reserved.

   This file is part of Elemental.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    - Neither the name of the owner nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/
#include "elemental/blas_internal.hpp"
using namespace std;
using namespace elemental;
using namespace elemental::imports::mpi;

// Template conventions:
//   G: general datatype
//
//   T: any ring, e.g., the (Gaussian) integers and the real/complex numbers
//   Z: representation of a real ring, e.g., the integers or real numbers
//   std::complex<Z>: representation of a complex ring, e.g. Gaussian integers
//                    or complex numbers
//
//   F: representation of real or complex number
//   R: representation of real number
//   std::complex<R>: representation of complex number

/* 
   C++ does not (currently) allow for partial function template specialization,
   so implementing Dot will be unnecessarily obfuscated. Sorry.
  
   The compromise is to have the user-level routine, 
  
     template<typename T, Distribution U, Distribution V,
                          Distribution W, Distribution Z >
     T
     blas::Dot( const DistMatrix<T,U,V>& x, const DistMatrix<T,W,Z>& y );

   simply route to overloaded pseudo-partial-specializations within the 
   blas::Internal namespace. For example,

     template<typename T, Distribution U, Distribution V>
     T
     blas::internal::Dot
     ( const DistMatrix<T,U,V>& x, const DistMatrix<T,MC,MR>& y );
*/
template<typename T, Distribution U, Distribution V,
                     Distribution W, Distribution Z >
T
elemental::blas::Dot
( const DistMatrix<T,U,V>& x, const DistMatrix<T,W,Z>& y )
{
#ifndef RELEASE
    PushCallStack("blas::Dot");
#endif
    T dotProduct = blas::internal::Dot( x, y );
#ifndef RELEASE
    PopCallStack();
#endif
    return dotProduct;
}

template<typename T, Distribution U, Distribution V>
T
elemental::blas::internal::Dot
( const DistMatrix<T,U,V>& x, const DistMatrix<T,MC,MR>& y )
{
#ifndef RELEASE
    PushCallStack("blas::internal::Dot");
    if( x.Grid() != y.Grid() )
        throw logic_error( "x and y must be distributed over the same grid." );
    if( (x.Height() != 1 && x.Width() != 1) ||
        (y.Height() != 1 && y.Width() != 1) )
        throw logic_error( "Dot requires x and y to be vectors." );
    int xLength = ( x.Width() == 1 ? x.Height() : x.Width() );
    int yLength = ( y.Width() == 1 ? y.Height() : y.Width() );
    if( xLength != yLength )
        throw logic_error( "Dot requires x and y to be the same length." );
#endif
    const Grid& g = x.Grid();

    T globalDot;
    if( x.Width() == 1 && y.Width() == 1 )
    {
        DistMatrix<T,MC,MR> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerCol = y.RowAlignment();
        if( g.MRRank() == ownerCol )
        { 
            T localDot = blas::Dot
                         ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
            AllReduce
            ( &localDot, &globalDot, 1, MPI_SUM, g.MCComm() );
        }
        Broadcast( &globalDot, 1, ownerCol, g.MRComm() );
    }
    else if( x.Width() == 1 )
    {
        DistMatrix<T,MR,MC> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerRow = y.ColAlignment();
        if( g.MCRank() == ownerRow )
        {
            T localDot = blas::Dot
                         ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
            AllReduce
            ( &localDot, &globalDot, 1, MPI_SUM, g.MRComm() );
        }
        Broadcast( &globalDot, 1, ownerRow, g.MCComm() );
    }
    else if( y.Width() == 1 )
    {
        DistMatrix<T,MR,MC> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerCol = y.RowAlignment();
        if( g.MRRank() == ownerCol )
        {
            T localDot = blas::Dot
                         ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
            AllReduce
            ( &localDot, &globalDot, 1, MPI_SUM, g.MCComm() );
        }
        Broadcast( &globalDot, 1, ownerCol, g.MRComm() );
    }
    else
    {
        DistMatrix<T,MC,MR> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerRow = y.ColAlignment();
        if( g.MCRank() == ownerRow )
        {
            T localDot = blas::Dot
                         ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
            AllReduce
            ( &localDot, &globalDot, 1, MPI_SUM, g.MRComm() );
        }
        Broadcast( &globalDot, 1, ownerRow, g.MCComm() );
    }
#ifndef RELEASE
    PopCallStack();
#endif
    return globalDot;
}

#ifdef ENABLE_ALL_DISTRIBUTED_DOT
template<typename T, Distribution U, Distribution V>
T
elemental::blas::internal::Dot
( const DistMatrix<T,U,V>& x, const DistMatrix<T,MC,Star>& y )
{
#ifndef RELEASE
    PushCallStack("blas::internal::Dot");
    if( x.Grid() != y.Grid() )
        throw logic_error( "x and y must be distributed over the same grid." );
    if( (x.Height() != 1 && x.Width() != 1) ||
        (y.Height() != 1 && y.Width() != 1) )
        throw logic_error( "Dot requires x and y to be vectors." );
    int xLength = ( x.Width() == 1 ? x.Height() : x.Width() );
    int yLength = ( y.Width() == 1 ? y.Height() : y.Width() );
    if( xLength != yLength )
        throw logic_error( "Dot requires x and y to be the same length." );
#endif
    const Grid& g = x.Grid();

    T globalDot;
    if( x.Width() == 1 && y.Width() == 1 )
    {
        DistMatrix<T,MC,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.MCComm() );
    }
    else if( x.Width() == 1 )
    {
        DistMatrix<T,Star,MC> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerRow = y.ColAlignment();
        if( g.MCRank() == ownerRow )
        {
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, ownerRow, g.MCComm() );
    }
    else if( y.Width() == 1 )
    {
        DistMatrix<T,Star,MC> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.MCComm() );
    }
    else
    {
        DistMatrix<T,MC,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerRow = y.ColAlignment();
        if( g.MCRank() == ownerRow )
        {
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, ownerRow, g.MCComm() );
    }
#ifndef RELEASE
    PopCallStack();
#endif
    return globalDot;
}

template<typename T, Distribution U, Distribution V>
T
elemental::blas::internal::Dot
( const DistMatrix<T,U,V>& x, const DistMatrix<T,Star,MR>& y )
{
#ifndef RELEASE
    PushCallStack("blas::internal::Dot");
    if( x.Grid() != y.Grid() )
        throw logic_error( "x and y must be distributed over the same grid." );
    if( (x.Height() != 1 && x.Width() != 1) ||
        (y.Height() != 1 && y.Width() != 1) )
        throw logic_error( "Dot requires x and y to be vectors." );
    int xLength = ( x.Width() == 1 ? x.Height() : x.Width() );
    int yLength = ( y.Width() == 1 ? y.Height() : y.Width() );
    if( xLength != yLength )
        throw logic_error( "Dot requires x and y to be the same length." );
#endif
    const Grid& g = x.Grid();

    T globalDot;
    if( x.Width() == 1 && y.Width() == 1 )
    {
        DistMatrix<T,Star,MR> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerCol = y.RowAlignment();
        if( g.MRRank() == ownerCol )
        { 
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, ownerCol, g.MRComm() );
    }
    else if( x.Width() == 1 )
    {
        DistMatrix<T,MR,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.MRComm() );
    }
    else if( y.Width() == 1 )
    {
        DistMatrix<T,MR,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerCol = y.RowAlignment();
        if( g.MRRank() == ownerCol )
        {
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, ownerCol, g.MRComm() );
    }
    else
    {
        DistMatrix<T,Star,MR> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.MRComm() );
    }
#ifndef RELEASE
    PopCallStack();
#endif
    return globalDot;
}

template<typename T, Distribution U, Distribution V>
T
elemental::blas::internal::Dot
( const DistMatrix<T,U,V>& x, const DistMatrix<T,MR,MC>& y )
{
#ifndef RELEASE
    PushCallStack("blas::internal::Dot");
    if( x.Grid() != y.Grid() )
        throw logic_error( "x and y must be distributed over the same grid." );
    if( (x.Height() != 1 && x.Width() != 1) ||
        (y.Height() != 1 && y.Width() != 1) )
        throw logic_error( "Dot requires x and y to be vectors." );
    int xLength = ( x.Width() == 1 ? x.Height() : x.Width() );
    int yLength = ( y.Width() == 1 ? y.Height() : y.Width() );
    if( xLength != yLength )
        throw logic_error( "Dot requires x and y to be the same length." );
#endif
    const Grid& g = x.Grid();

    T globalDot;
    if( x.Width() == 1 && y.Width() == 1 )
    {
        DistMatrix<T,MR,MC> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerRow = y.RowAlignment();
        if( g.MCRank() == ownerRow )
        { 
            T localDot = blas::Dot
                         ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
            AllReduce
            ( &localDot, &globalDot, 1, MPI_SUM, g.MRComm() );
        }
        Broadcast( &globalDot, 1, ownerRow, g.MCComm() );
    }
    else if( x.Width() == 1 )
    {
        DistMatrix<T,MC,MR> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerCol = y.ColAlignment();
        if( g.MRRank() == ownerCol )
        {
            T localDot = blas::Dot
                         ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
            AllReduce
            ( &localDot, &globalDot, 1, MPI_SUM, g.MCComm() );
        }
        Broadcast( &globalDot, 1, ownerCol, g.MRComm() );
    }
    else if( y.Width() == 1 )
    {
        DistMatrix<T,MC,MR> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerRow = y.RowAlignment();
        if( g.MCRank() == ownerRow )
        {
            T localDot = blas::Dot
                         ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
            AllReduce
            ( &localDot, &globalDot, 1, MPI_SUM, g.MRComm() );
        }
        Broadcast( &globalDot, 1, ownerRow, g.MCComm() );
    }
    else
    {
        DistMatrix<T,MR,MC> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerCol = y.ColAlignment();
        if( g.MRRank() == ownerCol )
        {
            T localDot = blas::Dot
                         ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
            AllReduce
            ( &localDot, &globalDot, 1, MPI_SUM, g.MCComm() );
        }
        Broadcast( &globalDot, 1, ownerCol, g.MRComm() );
    }
#ifndef RELEASE
    PopCallStack();
#endif
    return globalDot;
}

template<typename T, Distribution U, Distribution V>
T
elemental::blas::internal::Dot
( const DistMatrix<T,U,V>& x, const DistMatrix<T,MR,Star>& y )
{
#ifndef RELEASE
    PushCallStack("blas::internal::Dot");
    if( x.Grid() != y.Grid() )
        throw logic_error( "x and y must be distributed over the same grid." );
    if( (x.Height() != 1 && x.Width() != 1) ||
        (y.Height() != 1 && y.Width() != 1) )
        throw logic_error( "Dot requires x and y to be vectors." );
    int xLength = ( x.Width() == 1 ? x.Height() : x.Width() );
    int yLength = ( y.Width() == 1 ? y.Height() : y.Width() );
    if( xLength != yLength )
        throw logic_error( "Dot requires x and y to be the same length." );
#endif
    const Grid& g = x.Grid();

    T globalDot;
    if( x.Width() == 1 && y.Width() == 1 )
    {
        DistMatrix<T,MR,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.MRComm() );
    }
    else if( x.Width() == 1 )
    {
        DistMatrix<T,Star,MR> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerCol = y.ColAlignment();
        if( g.MRRank() == ownerCol )
        {
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, ownerCol, g.MRComm() );
    }
    else if( y.Width() == 1 )
    {
        DistMatrix<T,Star,MR> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.MRComm() );
    }
    else
    {
        DistMatrix<T,MR,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerCol = y.ColAlignment();
        if( g.MRRank() == ownerCol )
        {
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, ownerCol, g.MRComm() );
    }
#ifndef RELEASE 
    PopCallStack();
#endif
    return globalDot;
}

template<typename T, Distribution U, Distribution V>
T
elemental::blas::internal::Dot
( const DistMatrix<T,U,V>& x, const DistMatrix<T,Star,MC>& y )
{
#ifndef RELEASE
    PushCallStack("blas::internal::Dot");
    if( x.Grid() != y.Grid() )
        throw logic_error( "x and y must be distributed over the same grid." );
    if( (x.Height() != 1 && x.Width() != 1) ||
        (y.Height() != 1 && y.Width() != 1) )
        throw logic_error( "Dot requires x and y to be vectors." );
    int xLength = ( x.Width() == 1 ? x.Height() : x.Width() );
    int yLength = ( y.Width() == 1 ? y.Height() : y.Width() );
    if( xLength != yLength )
        throw logic_error( "Dot requires x and y to be the same length." );
#endif
    const Grid& g = x.Grid();

    T globalDot;
    if( x.Width() == 1 && y.Width() == 1 )
    {
        DistMatrix<T,Star,MC> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerRow = y.RowAlignment();
        if( g.MCRank() == ownerRow )
        { 
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, ownerRow, g.MCComm() );
    }
    else if( x.Width() == 1 )
    {
        DistMatrix<T,MC,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.MCComm() );
    }
    else if( y.Width() == 1 )
    {
        DistMatrix<T,MC,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int ownerRow = y.RowAlignment();
        if( g.MCRank() == ownerRow )
        {
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, ownerRow, g.MCComm() );
    }
    else
    {
        DistMatrix<T,Star,MC> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.MCComm() );
    }
#ifndef RELEASE
    PopCallStack();
#endif
    return globalDot;
}

template<typename T, Distribution U, Distribution V>
T
elemental::blas::internal::Dot
( const DistMatrix<T,U,V>& x, const DistMatrix<T,VC,Star>& y )
{
#ifndef RELEASE
    PushCallStack("blas::internal::Dot");
    if( x.Grid() != y.Grid() )
        throw logic_error( "x and y must be distributed over the same grid." );
    if( (x.Height() != 1 && x.Width() != 1) ||
        (y.Height() != 1 && y.Width() != 1) )
        throw logic_error( "Dot requires x and y to be vectors." );
    int xLength = ( x.Width() == 1 ? x.Height() : x.Width() );
    int yLength = ( y.Width() == 1 ? y.Height() : y.Width() );
    if( xLength != yLength )
        throw logic_error( "Dot requires x and y to be the same length." );
#endif
    const Grid& g = x.Grid();

    T globalDot;
    if( x.Width() == 1 && y.Width() == 1 )
    {
        DistMatrix<T,VC,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.VCComm() );
    }
    else if( x.Width() == 1 )
    {
        DistMatrix<T,Star,VC> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int owner = y.ColAlignment();
        if( g.VCRank() == owner )
        {
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, owner, g.VCComm() );
    }
    else if( y.Width() == 1 )
    {
        DistMatrix<T,Star,VC> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.VCComm() );
    }
    else
    {
        DistMatrix<T,VC,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int owner = y.ColAlignment();
        if( g.VCRank() == owner )
        {
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, owner, g.VCComm() );
    }
#ifndef RELEASE
    PopCallStack();
#endif
    return globalDot;
}

template<typename T, Distribution U, Distribution V>
T
elemental::blas::internal::Dot
( const DistMatrix<T,U,V>& x, const DistMatrix<T,Star,VC>& y )
{
#ifndef RELEASE
    PushCallStack("blas::internal::Dot");
    if( x.Grid() != y.Grid() )
        throw logic_error( "x and y must be distributed over the same grid." );
    if( (x.Height() != 1 && x.Width() != 1) ||
        (y.Height() != 1 && y.Width() != 1) )
        throw logic_error( "Dot requires x and y to be vectors." );
    int xLength = ( x.Width() == 1 ? x.Height() : x.Width() );
    int yLength = ( y.Width() == 1 ? y.Height() : y.Width() );
    if( xLength != yLength )
        throw logic_error( "Dot requires x and y to be the same length." );
#endif
    const Grid& g = x.Grid();

    T globalDot;
    if( x.Width() == 1 && y.Width() == 1 )
    {
        DistMatrix<T,Star,VC> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int owner = y.RowAlignment();
        if( g.VCRank() == owner )
        { 
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, owner, g.VCComm() );
    }
    else if( x.Width() == 1 )
    {
        DistMatrix<T,VC,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.VCComm() );
    }
    else if( y.Width() == 1 )
    {
        DistMatrix<T,VC,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int owner = y.RowAlignment();
        if( g.VCRank() == owner )
        {
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, owner, g.VCComm() );
    }
    else
    {
        DistMatrix<T,Star,VC> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.VCComm() );
    }
#ifndef RELEASE
    PopCallStack();
#endif
    return globalDot;
}

template<typename T, Distribution U, Distribution V>
T
elemental::blas::internal::Dot
( const DistMatrix<T,U,V>& x, const DistMatrix<T,VR,Star>& y )
{
#ifndef RELEASE
    PushCallStack("blas::internal::Dot");
    if( x.Grid() != y.Grid() )
        throw logic_error( "x and y must be distributed over the same grid." );
    if( (x.Height() != 1 && x.Width() != 1) ||
        (y.Height() != 1 && y.Width() != 1) )
        throw logic_error( "Dot requires x and y to be vectors." );
    int xLength = ( x.Width() == 1 ? x.Height() : x.Width() );
    int yLength = ( y.Width() == 1 ? y.Height() : y.Width() );
    if( xLength != yLength )
        throw logic_error( "Dot requires x and y to be the same length." );
#endif
    const Grid& g = x.Grid();

    T globalDot;
    if( x.Width() == 1 && y.Width() == 1 )
    {
        DistMatrix<T,VR,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.VRComm() );
    }
    else if( x.Width() == 1 )
    {
        DistMatrix<T,Star,VR> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int owner = y.ColAlignment();
        if( g.VRRank() == owner )
        {
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, owner, g.VRComm() );
    }
    else if( y.Width() == 1 )
    {
        DistMatrix<T,Star,VR> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.VRComm() );
    }
    else
    {
        DistMatrix<T,VR,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int owner = y.ColAlignment();
        if( g.VRRank() == owner )
        {
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, owner, g.VRComm() );
    }
#ifndef RELEASE
    PopCallStack();
#endif
    return globalDot;
}

template<typename T, Distribution U, Distribution V>
T
elemental::blas::internal::Dot
( const DistMatrix<T,U,V>& x, const DistMatrix<T,Star,VR>& y )
{
#ifndef RELEASE
    PushCallStack("blas::internal::Dot");
    if( x.Grid() != y.Grid() )
        throw logic_error( "x and y must be distributed over the same grid." );
    if( (x.Height() != 1 && x.Width() != 1) ||
        (y.Height() != 1 && y.Width() != 1) )
        throw logic_error( "Dot requires x and y to be vectors." );
    int xLength = ( x.Width() == 1 ? x.Height() : x.Width() );
    int yLength = ( y.Width() == 1 ? y.Height() : y.Width() );
    if( xLength != yLength )
        throw logic_error( "Dot requires x and y to be the same length." );
#endif
    const Grid& g = x.Grid();

    T globalDot;
    if( x.Width() == 1 && y.Width() == 1 )
    {
        DistMatrix<T,Star,VR> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int owner = y.RowAlignment();
        if( g.VRRank() == owner )
        { 
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, owner, g.VRComm() );
    }
    else if( x.Width() == 1 )
    {
        DistMatrix<T,VR,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.VRComm() );
    }
    else if( y.Width() == 1 )
    {
        DistMatrix<T,VR,Star> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        int owner = y.RowAlignment();
        if( g.VRRank() == owner )
        {
            globalDot = blas::Dot
                        ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        }
        Broadcast( &globalDot, 1, owner, g.VRComm() );
    }
    else
    {
        DistMatrix<T,Star,VR> xRedist(g);
        xRedist.AlignWith( y );
        xRedist = x;

        T localDot = blas::Dot
                     ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
        AllReduce( &localDot, &globalDot, 1, MPI_SUM, g.VRComm() );
    }
#ifndef RELEASE
    PopCallStack();
#endif
    return globalDot;
}

template<typename T, Distribution U, Distribution V>
T
elemental::blas::internal::Dot
( const DistMatrix<T,U,V>& x, const DistMatrix<T,Star,Star>& y )
{
#ifndef RELEASE
    PushCallStack("blas::internal::Dot");
    if( x.Grid() != y.Grid() )
        throw logic_error( "x and y must be distributed over the same grid." );
    if( (x.Height() != 1 && x.Width() != 1) ||
        (y.Height() != 1 && y.Width() != 1) )
        throw logic_error( "Dot requires x and y to be vectors." );
    int xLength = ( x.Width() == 1 ? x.Height() : x.Width() );
    int yLength = ( y.Width() == 1 ? y.Height() : y.Width() );
    if( xLength != yLength )
        throw logic_error( "Dot requires x and y to be the same length." );
#endif
    const Grid& g = x.Grid();

    DistMatrix<T,Star,Star> xRedist(g);
    xRedist = x;

    T globalDot = blas::Dot
                  ( xRedist.LockedLocalMatrix(), y.LockedLocalMatrix() );
#ifndef RELEASE
    PopCallStack();
#endif
    return globalDot;
}
#endif // ENABLE_ALL_DISTRIBUTED_DOT

// We only need to explicitly instantiate the wrapper, since the underlying 
// routines in blas::Internal will be immediately called 
template float elemental::blas::Dot
( const DistMatrix<float,MC,MR>& x,
  const DistMatrix<float,MC,MR>& y );
#ifdef ENABLE_ALL_DISTRIBUTED_DOT
template float elemental::blas::Dot
( const DistMatrix<float,MC,MR>& x,
  const DistMatrix<float,MC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,MR>& x,
  const DistMatrix<float,Star,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,MR>& x,
  const DistMatrix<float,MR,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,MR>& x,
  const DistMatrix<float,MR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,MR>& x,
  const DistMatrix<float,Star,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,MR>& x,
  const DistMatrix<float,VC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,MR>& x,
  const DistMatrix<float,Star,VC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,MR>& x,
  const DistMatrix<float,VR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,MR>& x,
  const DistMatrix<float,Star,VR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,MR>& x,
  const DistMatrix<float,Star,Star>& y );

template float elemental::blas::Dot
( const DistMatrix<float,MC,Star>& x,
  const DistMatrix<float,MC,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,Star>& x,
  const DistMatrix<float,MC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,Star>& x,
  const DistMatrix<float,Star,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,Star>& x,
  const DistMatrix<float,MR,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,Star>& x,
  const DistMatrix<float,MR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,Star>& x,
  const DistMatrix<float,Star,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,Star>& x,
  const DistMatrix<float,VC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,Star>& x,
  const DistMatrix<float,Star,VC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,Star>& x,
  const DistMatrix<float,VR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,Star>& x,
  const DistMatrix<float,Star,VR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MC,Star>& x,
  const DistMatrix<float,Star,Star>& y );

template float elemental::blas::Dot
( const DistMatrix<float,Star,MR>& x,
  const DistMatrix<float,MC,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MR>& x,
  const DistMatrix<float,MC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MR>& x,
  const DistMatrix<float,Star,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MR>& x,
  const DistMatrix<float,MR,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MR>& x,
  const DistMatrix<float,MR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MR>& x,
  const DistMatrix<float,Star,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MR>& x,
  const DistMatrix<float,VC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MR>& x,
  const DistMatrix<float,Star,VC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MR>& x,
  const DistMatrix<float,VR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MR>& x,
  const DistMatrix<float,Star,VR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MR>& x,
  const DistMatrix<float,Star,Star>& y );

template float elemental::blas::Dot
( const DistMatrix<float,MR,MC>& x,
  const DistMatrix<float,MC,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,MC>& x,
  const DistMatrix<float,MC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,MC>& x,
  const DistMatrix<float,Star,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,MC>& x,
  const DistMatrix<float,MR,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,MC>& x,
  const DistMatrix<float,MR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,MC>& x,
  const DistMatrix<float,Star,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,MC>& x,
  const DistMatrix<float,VC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,MC>& x,
  const DistMatrix<float,Star,VC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,MC>& x,
  const DistMatrix<float,VR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,MC>& x,
  const DistMatrix<float,Star,VR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,MC>& x,
  const DistMatrix<float,Star,Star>& y );

template float elemental::blas::Dot
( const DistMatrix<float,MR,Star>& x,
  const DistMatrix<float,MC,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,Star>& x,
  const DistMatrix<float,MC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,Star>& x,
  const DistMatrix<float,Star,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,Star>& x,
  const DistMatrix<float,MR,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,Star>& x,
  const DistMatrix<float,MR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,Star>& x,
  const DistMatrix<float,Star,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,Star>& x,
  const DistMatrix<float,VC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,Star>& x,
  const DistMatrix<float,Star,VC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,Star>& x,
  const DistMatrix<float,VR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,Star>& x,
  const DistMatrix<float,Star,VR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,MR,Star>& x,
  const DistMatrix<float,Star,Star>& y );

template float elemental::blas::Dot
( const DistMatrix<float,Star,MC>& x,
  const DistMatrix<float,MC,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MC>& x,
  const DistMatrix<float,MC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MC>& x,
  const DistMatrix<float,Star,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MC>& x,
  const DistMatrix<float,MR,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MC>& x,
  const DistMatrix<float,MR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MC>& x,
  const DistMatrix<float,Star,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MC>& x,
  const DistMatrix<float,VC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MC>& x,
  const DistMatrix<float,Star,VC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MC>& x,
  const DistMatrix<float,VR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MC>& x,
  const DistMatrix<float,Star,VR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,MC>& x,
  const DistMatrix<float,Star,Star>& y );

template float elemental::blas::Dot
( const DistMatrix<float,VC,Star>& x,
  const DistMatrix<float,MC,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VC,Star>& x,
  const DistMatrix<float,MC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VC,Star>& x,
  const DistMatrix<float,Star,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VC,Star>& x,
  const DistMatrix<float,MR,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VC,Star>& x,
  const DistMatrix<float,MR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VC,Star>& x,
  const DistMatrix<float,Star,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VC,Star>& x,
  const DistMatrix<float,VC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VC,Star>& x,
  const DistMatrix<float,Star,VC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VC,Star>& x,
  const DistMatrix<float,VR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VC,Star>& x,
  const DistMatrix<float,Star,VR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VC,Star>& x,
  const DistMatrix<float,Star,Star>& y );

template float elemental::blas::Dot
( const DistMatrix<float,Star,VC>& x,
  const DistMatrix<float,MC,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VC>& x,
  const DistMatrix<float,MC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VC>& x,
  const DistMatrix<float,Star,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VC>& x,
  const DistMatrix<float,MR,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VC>& x,
  const DistMatrix<float,MR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VC>& x,
  const DistMatrix<float,Star,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VC>& x,
  const DistMatrix<float,VC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VC>& x,
  const DistMatrix<float,Star,VC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VC>& x,
  const DistMatrix<float,VR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VC>& x,
  const DistMatrix<float,Star,VR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VC>& x,
  const DistMatrix<float,Star,Star>& y );

template float elemental::blas::Dot
( const DistMatrix<float,VR,Star>& x,
  const DistMatrix<float,MC,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VR,Star>& x,
  const DistMatrix<float,MC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VR,Star>& x,
  const DistMatrix<float,Star,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VR,Star>& x,
  const DistMatrix<float,MR,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VR,Star>& x,
  const DistMatrix<float,MR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VR,Star>& x,
  const DistMatrix<float,Star,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VR,Star>& x,
  const DistMatrix<float,VC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VR,Star>& x,
  const DistMatrix<float,Star,VC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VR,Star>& x,
  const DistMatrix<float,VR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VR,Star>& x,
  const DistMatrix<float,Star,VR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,VR,Star>& x,
  const DistMatrix<float,Star,Star>& y );

template float elemental::blas::Dot
( const DistMatrix<float,Star,VR>& x,
  const DistMatrix<float,MC,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VR>& x,
  const DistMatrix<float,MC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VR>& x,
  const DistMatrix<float,Star,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VR>& x,
  const DistMatrix<float,MR,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VR>& x,
  const DistMatrix<float,MR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VR>& x,
  const DistMatrix<float,Star,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VR>& x,
  const DistMatrix<float,VC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VR>& x,
  const DistMatrix<float,Star,VC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VR>& x,
  const DistMatrix<float,VR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VR>& x,
  const DistMatrix<float,Star,VR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,VR>& x,
  const DistMatrix<float,Star,Star>& y );

template float elemental::blas::Dot
( const DistMatrix<float,Star,Star>& x,
  const DistMatrix<float,MC,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,Star>& x,
  const DistMatrix<float,MC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,Star>& x,
  const DistMatrix<float,Star,MR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,Star>& x,
  const DistMatrix<float,MR,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,Star>& x,
  const DistMatrix<float,MR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,Star>& x,
  const DistMatrix<float,Star,MC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,Star>& x,
  const DistMatrix<float,VC,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,Star>& x,
  const DistMatrix<float,Star,VC>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,Star>& x,
  const DistMatrix<float,VR,Star>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,Star>& x,
  const DistMatrix<float,Star,VR>& y );
template float elemental::blas::Dot
( const DistMatrix<float,Star,Star>& x,
  const DistMatrix<float,Star,Star>& y );
#endif // ENABLE_ALL_DISTRIBUTED_DOT

template double elemental::blas::Dot
( const DistMatrix<double,MC,MR>& x,
  const DistMatrix<double,MC,MR>& y );
#ifdef ENABLE_ALL_DISTRIBUTED_DOT
template double elemental::blas::Dot
( const DistMatrix<double,MC,MR>& x,
  const DistMatrix<double,MC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,MR>& x,
  const DistMatrix<double,Star,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,MR>& x,
  const DistMatrix<double,MR,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,MR>& x,
  const DistMatrix<double,MR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,MR>& x,
  const DistMatrix<double,Star,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,MR>& x,
  const DistMatrix<double,VC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,MR>& x,
  const DistMatrix<double,Star,VC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,MR>& x,
  const DistMatrix<double,VR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,MR>& x,
  const DistMatrix<double,Star,VR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,MR>& x,
  const DistMatrix<double,Star,Star>& y );

template double elemental::blas::Dot
( const DistMatrix<double,MC,Star>& x,
  const DistMatrix<double,MC,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,Star>& x,
  const DistMatrix<double,MC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,Star>& x,
  const DistMatrix<double,Star,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,Star>& x,
  const DistMatrix<double,MR,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,Star>& x,
  const DistMatrix<double,MR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,Star>& x,
  const DistMatrix<double,Star,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,Star>& x,
  const DistMatrix<double,VC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,Star>& x,
  const DistMatrix<double,Star,VC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,Star>& x,
  const DistMatrix<double,VR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,Star>& x,
  const DistMatrix<double,Star,VR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MC,Star>& x,
  const DistMatrix<double,Star,Star>& y );

template double elemental::blas::Dot
( const DistMatrix<double,Star,MR>& x,
  const DistMatrix<double,MC,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MR>& x,
  const DistMatrix<double,MC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MR>& x,
  const DistMatrix<double,Star,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MR>& x,
  const DistMatrix<double,MR,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MR>& x,
  const DistMatrix<double,MR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MR>& x,
  const DistMatrix<double,Star,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MR>& x,
  const DistMatrix<double,VC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MR>& x,
  const DistMatrix<double,Star,VC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MR>& x,
  const DistMatrix<double,VR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MR>& x,
  const DistMatrix<double,Star,VR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MR>& x,
  const DistMatrix<double,Star,Star>& y );

template double elemental::blas::Dot
( const DistMatrix<double,MR,MC>& x,
  const DistMatrix<double,MC,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,MC>& x,
  const DistMatrix<double,MC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,MC>& x,
  const DistMatrix<double,Star,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,MC>& x,
  const DistMatrix<double,MR,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,MC>& x,
  const DistMatrix<double,MR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,MC>& x,
  const DistMatrix<double,Star,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,MC>& x,
  const DistMatrix<double,VC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,MC>& x,
  const DistMatrix<double,Star,VC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,MC>& x,
  const DistMatrix<double,VR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,MC>& x,
  const DistMatrix<double,Star,VR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,MC>& x,
  const DistMatrix<double,Star,Star>& y );

template double elemental::blas::Dot
( const DistMatrix<double,MR,Star>& x,
  const DistMatrix<double,MC,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,Star>& x,
  const DistMatrix<double,MC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,Star>& x,
  const DistMatrix<double,Star,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,Star>& x,
  const DistMatrix<double,MR,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,Star>& x,
  const DistMatrix<double,MR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,Star>& x,
  const DistMatrix<double,Star,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,Star>& x,
  const DistMatrix<double,VC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,Star>& x,
  const DistMatrix<double,Star,VC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,Star>& x,
  const DistMatrix<double,VR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,Star>& x,
  const DistMatrix<double,Star,VR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,MR,Star>& x,
  const DistMatrix<double,Star,Star>& y );

template double elemental::blas::Dot
( const DistMatrix<double,Star,MC>& x,
  const DistMatrix<double,MC,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MC>& x,
  const DistMatrix<double,MC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MC>& x,
  const DistMatrix<double,Star,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MC>& x,
  const DistMatrix<double,MR,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MC>& x,
  const DistMatrix<double,MR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MC>& x,
  const DistMatrix<double,Star,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MC>& x,
  const DistMatrix<double,VC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MC>& x,
  const DistMatrix<double,Star,VC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MC>& x,
  const DistMatrix<double,VR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MC>& x,
  const DistMatrix<double,Star,VR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,MC>& x,
  const DistMatrix<double,Star,Star>& y );

template double elemental::blas::Dot
( const DistMatrix<double,VC,Star>& x,
  const DistMatrix<double,MC,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VC,Star>& x,
  const DistMatrix<double,MC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VC,Star>& x,
  const DistMatrix<double,Star,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VC,Star>& x,
  const DistMatrix<double,MR,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VC,Star>& x,
  const DistMatrix<double,MR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VC,Star>& x,
  const DistMatrix<double,Star,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VC,Star>& x,
  const DistMatrix<double,VC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VC,Star>& x,
  const DistMatrix<double,Star,VC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VC,Star>& x,
  const DistMatrix<double,VR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VC,Star>& x,
  const DistMatrix<double,Star,VR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VC,Star>& x,
  const DistMatrix<double,Star,Star>& y );

template double elemental::blas::Dot
( const DistMatrix<double,Star,VC>& x,
  const DistMatrix<double,MC,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VC>& x,
  const DistMatrix<double,MC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VC>& x,
  const DistMatrix<double,Star,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VC>& x,
  const DistMatrix<double,MR,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VC>& x,
  const DistMatrix<double,MR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VC>& x,
  const DistMatrix<double,Star,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VC>& x,
  const DistMatrix<double,VC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VC>& x,
  const DistMatrix<double,Star,VC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VC>& x,
  const DistMatrix<double,VR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VC>& x,
  const DistMatrix<double,Star,VR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VC>& x,
  const DistMatrix<double,Star,Star>& y );

template double elemental::blas::Dot
( const DistMatrix<double,VR,Star>& x,
  const DistMatrix<double,MC,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VR,Star>& x,
  const DistMatrix<double,MC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VR,Star>& x,
  const DistMatrix<double,Star,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VR,Star>& x,
  const DistMatrix<double,MR,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VR,Star>& x,
  const DistMatrix<double,MR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VR,Star>& x,
  const DistMatrix<double,Star,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VR,Star>& x,
  const DistMatrix<double,VC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VR,Star>& x,
  const DistMatrix<double,Star,VC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VR,Star>& x,
  const DistMatrix<double,VR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VR,Star>& x,
  const DistMatrix<double,Star,VR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,VR,Star>& x,
  const DistMatrix<double,Star,Star>& y );

template double elemental::blas::Dot
( const DistMatrix<double,Star,VR>& x,
  const DistMatrix<double,MC,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VR>& x,
  const DistMatrix<double,MC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VR>& x,
  const DistMatrix<double,Star,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VR>& x,
  const DistMatrix<double,MR,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VR>& x,
  const DistMatrix<double,MR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VR>& x,
  const DistMatrix<double,Star,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VR>& x,
  const DistMatrix<double,VC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VR>& x,
  const DistMatrix<double,Star,VC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VR>& x,
  const DistMatrix<double,VR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VR>& x,
  const DistMatrix<double,Star,VR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,VR>& x,
  const DistMatrix<double,Star,Star>& y );

template double elemental::blas::Dot
( const DistMatrix<double,Star,Star>& x,
  const DistMatrix<double,MC,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,Star>& x,
  const DistMatrix<double,MC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,Star>& x,
  const DistMatrix<double,Star,MR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,Star>& x,
  const DistMatrix<double,MR,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,Star>& x,
  const DistMatrix<double,MR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,Star>& x,
  const DistMatrix<double,Star,MC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,Star>& x,
  const DistMatrix<double,VC,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,Star>& x,
  const DistMatrix<double,Star,VC>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,Star>& x,
  const DistMatrix<double,VR,Star>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,Star>& x,
  const DistMatrix<double,Star,VR>& y );
template double elemental::blas::Dot
( const DistMatrix<double,Star,Star>& x,
  const DistMatrix<double,Star,Star>& y );
#endif // ENABLE_ALL_DISTRIBUTED_DOT

#ifndef WITHOUT_COMPLEX
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,MR>& x,
  const DistMatrix<scomplex,MC,MR>& y );
#ifdef ENABLE_ALL_DISTRIBUTED_DOT
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,MR>& x,
  const DistMatrix<scomplex,MC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,MR>& x,
  const DistMatrix<scomplex,Star,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,MR>& x,
  const DistMatrix<scomplex,MR,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,MR>& x,
  const DistMatrix<scomplex,MR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,MR>& x,
  const DistMatrix<scomplex,Star,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,MR>& x,
  const DistMatrix<scomplex,VC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,MR>& x,
  const DistMatrix<scomplex,Star,VC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,MR>& x,
  const DistMatrix<scomplex,VR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,MR>& x,
  const DistMatrix<scomplex,Star,VR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,MR>& x,
  const DistMatrix<scomplex,Star,Star>& y );

template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,Star>& x,
  const DistMatrix<scomplex,MC,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,Star>& x,
  const DistMatrix<scomplex,MC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,Star>& x,
  const DistMatrix<scomplex,Star,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,Star>& x,
  const DistMatrix<scomplex,MR,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,Star>& x,
  const DistMatrix<scomplex,MR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,Star>& x,
  const DistMatrix<scomplex,Star,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,Star>& x,
  const DistMatrix<scomplex,VC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,Star>& x,
  const DistMatrix<scomplex,Star,VC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,Star>& x,
  const DistMatrix<scomplex,VR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,Star>& x,
  const DistMatrix<scomplex,Star,VR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MC,Star>& x,
  const DistMatrix<scomplex,Star,Star>& y );

template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MR>& x,
  const DistMatrix<scomplex,MC,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MR>& x,
  const DistMatrix<scomplex,MC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MR>& x,
  const DistMatrix<scomplex,Star,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MR>& x,
  const DistMatrix<scomplex,MR,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MR>& x,
  const DistMatrix<scomplex,MR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MR>& x,
  const DistMatrix<scomplex,Star,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MR>& x,
  const DistMatrix<scomplex,VC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MR>& x,
  const DistMatrix<scomplex,Star,VC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MR>& x,
  const DistMatrix<scomplex,VR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MR>& x,
  const DistMatrix<scomplex,Star,VR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MR>& x,
  const DistMatrix<scomplex,Star,Star>& y );

template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,MC>& x,
  const DistMatrix<scomplex,MC,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,MC>& x,
  const DistMatrix<scomplex,MC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,MC>& x,
  const DistMatrix<scomplex,Star,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,MC>& x,
  const DistMatrix<scomplex,MR,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,MC>& x,
  const DistMatrix<scomplex,MR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,MC>& x,
  const DistMatrix<scomplex,Star,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,MC>& x,
  const DistMatrix<scomplex,VC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,MC>& x,
  const DistMatrix<scomplex,Star,VC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,MC>& x,
  const DistMatrix<scomplex,VR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,MC>& x,
  const DistMatrix<scomplex,Star,VR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,MC>& x,
  const DistMatrix<scomplex,Star,Star>& y );

template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,Star>& x,
  const DistMatrix<scomplex,MC,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,Star>& x,
  const DistMatrix<scomplex,MC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,Star>& x,
  const DistMatrix<scomplex,Star,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,Star>& x,
  const DistMatrix<scomplex,MR,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,Star>& x,
  const DistMatrix<scomplex,MR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,Star>& x,
  const DistMatrix<scomplex,Star,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,Star>& x,
  const DistMatrix<scomplex,VC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,Star>& x,
  const DistMatrix<scomplex,Star,VC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,Star>& x,
  const DistMatrix<scomplex,VR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,Star>& x,
  const DistMatrix<scomplex,Star,VR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,MR,Star>& x,
  const DistMatrix<scomplex,Star,Star>& y );

template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MC>& x,
  const DistMatrix<scomplex,MC,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MC>& x,
  const DistMatrix<scomplex,MC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MC>& x,
  const DistMatrix<scomplex,Star,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MC>& x,
  const DistMatrix<scomplex,MR,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MC>& x,
  const DistMatrix<scomplex,MR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MC>& x,
  const DistMatrix<scomplex,Star,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MC>& x,
  const DistMatrix<scomplex,VC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MC>& x,
  const DistMatrix<scomplex,Star,VC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MC>& x,
  const DistMatrix<scomplex,VR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MC>& x,
  const DistMatrix<scomplex,Star,VR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,MC>& x,
  const DistMatrix<scomplex,Star,Star>& y );

template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VC,Star>& x,
  const DistMatrix<scomplex,MC,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VC,Star>& x,
  const DistMatrix<scomplex,MC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VC,Star>& x,
  const DistMatrix<scomplex,Star,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VC,Star>& x,
  const DistMatrix<scomplex,MR,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VC,Star>& x,
  const DistMatrix<scomplex,MR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VC,Star>& x,
  const DistMatrix<scomplex,Star,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VC,Star>& x,
  const DistMatrix<scomplex,VC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VC,Star>& x,
  const DistMatrix<scomplex,Star,VC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VC,Star>& x,
  const DistMatrix<scomplex,VR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VC,Star>& x,
  const DistMatrix<scomplex,Star,VR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VC,Star>& x,
  const DistMatrix<scomplex,Star,Star>& y );

template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VC>& x,
  const DistMatrix<scomplex,MC,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VC>& x,
  const DistMatrix<scomplex,MC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VC>& x,
  const DistMatrix<scomplex,Star,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VC>& x,
  const DistMatrix<scomplex,MR,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VC>& x,
  const DistMatrix<scomplex,MR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VC>& x,
  const DistMatrix<scomplex,Star,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VC>& x,
  const DistMatrix<scomplex,VC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VC>& x,
  const DistMatrix<scomplex,Star,VC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VC>& x,
  const DistMatrix<scomplex,VR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VC>& x,
  const DistMatrix<scomplex,Star,VR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VC>& x,
  const DistMatrix<scomplex,Star,Star>& y );

template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VR,Star>& x,
  const DistMatrix<scomplex,MC,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VR,Star>& x,
  const DistMatrix<scomplex,MC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VR,Star>& x,
  const DistMatrix<scomplex,Star,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VR,Star>& x,
  const DistMatrix<scomplex,MR,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VR,Star>& x,
  const DistMatrix<scomplex,MR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VR,Star>& x,
  const DistMatrix<scomplex,Star,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VR,Star>& x,
  const DistMatrix<scomplex,VC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VR,Star>& x,
  const DistMatrix<scomplex,Star,VC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VR,Star>& x,
  const DistMatrix<scomplex,VR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VR,Star>& x,
  const DistMatrix<scomplex,Star,VR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,VR,Star>& x,
  const DistMatrix<scomplex,Star,Star>& y );

template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VR>& x,
  const DistMatrix<scomplex,MC,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VR>& x,
  const DistMatrix<scomplex,MC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VR>& x,
  const DistMatrix<scomplex,Star,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VR>& x,
  const DistMatrix<scomplex,MR,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VR>& x,
  const DistMatrix<scomplex,MR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VR>& x,
  const DistMatrix<scomplex,Star,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VR>& x,
  const DistMatrix<scomplex,VC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VR>& x,
  const DistMatrix<scomplex,Star,VC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VR>& x,
  const DistMatrix<scomplex,VR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VR>& x,
  const DistMatrix<scomplex,Star,VR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,VR>& x,
  const DistMatrix<scomplex,Star,Star>& y );

template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,Star>& x,
  const DistMatrix<scomplex,MC,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,Star>& x,
  const DistMatrix<scomplex,MC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,Star>& x,
  const DistMatrix<scomplex,Star,MR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,Star>& x,
  const DistMatrix<scomplex,MR,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,Star>& x,
  const DistMatrix<scomplex,MR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,Star>& x,
  const DistMatrix<scomplex,Star,MC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,Star>& x,
  const DistMatrix<scomplex,VC,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,Star>& x,
  const DistMatrix<scomplex,Star,VC>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,Star>& x,
  const DistMatrix<scomplex,VR,Star>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,Star>& x,
  const DistMatrix<scomplex,Star,VR>& y );
template scomplex elemental::blas::Dot
( const DistMatrix<scomplex,Star,Star>& x,
  const DistMatrix<scomplex,Star,Star>& y );
#endif // ENABLE_ALL_DISTRIBUTED_DOT

template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,MR>& x,
  const DistMatrix<dcomplex,MC,MR>& y );
#ifdef ENABLE_ALL_DISTRIBUTED_DOT
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,MR>& x,
  const DistMatrix<dcomplex,MC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,MR>& x,
  const DistMatrix<dcomplex,Star,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,MR>& x,
  const DistMatrix<dcomplex,MR,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,MR>& x,
  const DistMatrix<dcomplex,MR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,MR>& x,
  const DistMatrix<dcomplex,Star,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,MR>& x,
  const DistMatrix<dcomplex,VC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,MR>& x,
  const DistMatrix<dcomplex,Star,VC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,MR>& x,
  const DistMatrix<dcomplex,VR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,MR>& x,
  const DistMatrix<dcomplex,Star,VR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,MR>& x,
  const DistMatrix<dcomplex,Star,Star>& y );

template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,Star>& x,
  const DistMatrix<dcomplex,MC,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,Star>& x,
  const DistMatrix<dcomplex,MC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,Star>& x,
  const DistMatrix<dcomplex,Star,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,Star>& x,
  const DistMatrix<dcomplex,MR,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,Star>& x,
  const DistMatrix<dcomplex,MR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,Star>& x,
  const DistMatrix<dcomplex,Star,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,Star>& x,
  const DistMatrix<dcomplex,VC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,Star>& x,
  const DistMatrix<dcomplex,Star,VC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,Star>& x,
  const DistMatrix<dcomplex,VR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,Star>& x,
  const DistMatrix<dcomplex,Star,VR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MC,Star>& x,
  const DistMatrix<dcomplex,Star,Star>& y );

template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MR>& x,
  const DistMatrix<dcomplex,MC,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MR>& x,
  const DistMatrix<dcomplex,MC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MR>& x,
  const DistMatrix<dcomplex,Star,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MR>& x,
  const DistMatrix<dcomplex,MR,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MR>& x,
  const DistMatrix<dcomplex,MR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MR>& x,
  const DistMatrix<dcomplex,Star,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MR>& x,
  const DistMatrix<dcomplex,VC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MR>& x,
  const DistMatrix<dcomplex,Star,VC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MR>& x,
  const DistMatrix<dcomplex,VR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MR>& x,
  const DistMatrix<dcomplex,Star,VR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MR>& x,
  const DistMatrix<dcomplex,Star,Star>& y );

template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,MC>& x,
  const DistMatrix<dcomplex,MC,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,MC>& x,
  const DistMatrix<dcomplex,MC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,MC>& x,
  const DistMatrix<dcomplex,Star,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,MC>& x,
  const DistMatrix<dcomplex,MR,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,MC>& x,
  const DistMatrix<dcomplex,MR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,MC>& x,
  const DistMatrix<dcomplex,Star,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,MC>& x,
  const DistMatrix<dcomplex,VC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,MC>& x,
  const DistMatrix<dcomplex,Star,VC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,MC>& x,
  const DistMatrix<dcomplex,VR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,MC>& x,
  const DistMatrix<dcomplex,Star,VR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,MC>& x,
  const DistMatrix<dcomplex,Star,Star>& y );

template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,Star>& x,
  const DistMatrix<dcomplex,MC,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,Star>& x,
  const DistMatrix<dcomplex,MC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,Star>& x,
  const DistMatrix<dcomplex,Star,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,Star>& x,
  const DistMatrix<dcomplex,MR,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,Star>& x,
  const DistMatrix<dcomplex,MR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,Star>& x,
  const DistMatrix<dcomplex,Star,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,Star>& x,
  const DistMatrix<dcomplex,VC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,Star>& x,
  const DistMatrix<dcomplex,Star,VC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,Star>& x,
  const DistMatrix<dcomplex,VR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,Star>& x,
  const DistMatrix<dcomplex,Star,VR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,MR,Star>& x,
  const DistMatrix<dcomplex,Star,Star>& y );

template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MC>& x,
  const DistMatrix<dcomplex,MC,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MC>& x,
  const DistMatrix<dcomplex,MC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MC>& x,
  const DistMatrix<dcomplex,Star,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MC>& x,
  const DistMatrix<dcomplex,MR,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MC>& x,
  const DistMatrix<dcomplex,MR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MC>& x,
  const DistMatrix<dcomplex,Star,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MC>& x,
  const DistMatrix<dcomplex,VC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MC>& x,
  const DistMatrix<dcomplex,Star,VC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MC>& x,
  const DistMatrix<dcomplex,VR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MC>& x,
  const DistMatrix<dcomplex,Star,VR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,MC>& x,
  const DistMatrix<dcomplex,Star,Star>& y );

template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VC,Star>& x,
  const DistMatrix<dcomplex,MC,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VC,Star>& x,
  const DistMatrix<dcomplex,MC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VC,Star>& x,
  const DistMatrix<dcomplex,Star,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VC,Star>& x,
  const DistMatrix<dcomplex,MR,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VC,Star>& x,
  const DistMatrix<dcomplex,MR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VC,Star>& x,
  const DistMatrix<dcomplex,Star,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VC,Star>& x,
  const DistMatrix<dcomplex,VC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VC,Star>& x,
  const DistMatrix<dcomplex,Star,VC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VC,Star>& x,
  const DistMatrix<dcomplex,VR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VC,Star>& x,
  const DistMatrix<dcomplex,Star,VR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VC,Star>& x,
  const DistMatrix<dcomplex,Star,Star>& y );

template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VC>& x,
  const DistMatrix<dcomplex,MC,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VC>& x,
  const DistMatrix<dcomplex,MC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VC>& x,
  const DistMatrix<dcomplex,Star,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VC>& x,
  const DistMatrix<dcomplex,MR,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VC>& x,
  const DistMatrix<dcomplex,MR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VC>& x,
  const DistMatrix<dcomplex,Star,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VC>& x,
  const DistMatrix<dcomplex,VC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VC>& x,
  const DistMatrix<dcomplex,Star,VC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VC>& x,
  const DistMatrix<dcomplex,VR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VC>& x,
  const DistMatrix<dcomplex,Star,VR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VC>& x,
  const DistMatrix<dcomplex,Star,Star>& y );

template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VR,Star>& x,
  const DistMatrix<dcomplex,MC,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VR,Star>& x,
  const DistMatrix<dcomplex,MC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VR,Star>& x,
  const DistMatrix<dcomplex,Star,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VR,Star>& x,
  const DistMatrix<dcomplex,MR,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VR,Star>& x,
  const DistMatrix<dcomplex,MR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VR,Star>& x,
  const DistMatrix<dcomplex,Star,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VR,Star>& x,
  const DistMatrix<dcomplex,VC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VR,Star>& x,
  const DistMatrix<dcomplex,Star,VC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VR,Star>& x,
  const DistMatrix<dcomplex,VR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VR,Star>& x,
  const DistMatrix<dcomplex,Star,VR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,VR,Star>& x,
  const DistMatrix<dcomplex,Star,Star>& y );

template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VR>& x,
  const DistMatrix<dcomplex,MC,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VR>& x,
  const DistMatrix<dcomplex,MC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VR>& x,
  const DistMatrix<dcomplex,Star,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VR>& x,
  const DistMatrix<dcomplex,MR,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VR>& x,
  const DistMatrix<dcomplex,MR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VR>& x,
  const DistMatrix<dcomplex,Star,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VR>& x,
  const DistMatrix<dcomplex,VC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VR>& x,
  const DistMatrix<dcomplex,Star,VC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VR>& x,
  const DistMatrix<dcomplex,VR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VR>& x,
  const DistMatrix<dcomplex,Star,VR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,VR>& x,
  const DistMatrix<dcomplex,Star,Star>& y );

template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,Star>& x,
  const DistMatrix<dcomplex,MC,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,Star>& x,
  const DistMatrix<dcomplex,MC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,Star>& x,
  const DistMatrix<dcomplex,Star,MR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,Star>& x,
  const DistMatrix<dcomplex,MR,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,Star>& x,
  const DistMatrix<dcomplex,MR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,Star>& x,
  const DistMatrix<dcomplex,Star,MC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,Star>& x,
  const DistMatrix<dcomplex,VC,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,Star>& x,
  const DistMatrix<dcomplex,Star,VC>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,Star>& x,
  const DistMatrix<dcomplex,VR,Star>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,Star>& x,
  const DistMatrix<dcomplex,Star,VR>& y );
template dcomplex elemental::blas::Dot
( const DistMatrix<dcomplex,Star,Star>& x,
  const DistMatrix<dcomplex,Star,Star>& y );
#endif // ENABLE_ALL_DISTRIBUTED_DOT
#endif // WITHOUT_COMPLEX

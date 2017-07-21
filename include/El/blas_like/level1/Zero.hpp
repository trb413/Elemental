/*
   Copyright (c) 2009-2016, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef EL_BLAS_ZERO_HPP
#define EL_BLAS_ZERO_HPP

namespace El {

template<typename T>
void Zero( Matrix<T>& A )
{
    EL_DEBUG_CSE
    const Int height = A.Height();
    const Int width = A.Width();
    const Int ALDim = A.LDim();
    T* ABuf = A.Buffer();

    // Zero out all entries if memory is contiguous. Otherwise zero
    // out each column.
    if( ALDim == height )
    {
        MemZero( ABuf, height*width );
    }
    else
    {
        EL_PARALLEL_FOR
        for( Int j=0; j<width; ++j )
        {
            MemZero( &ABuf[j*ALDim], height );
        }
    }

}

template<typename T>
void Zero( AbstractDistMatrix<T>& A )
{
    EL_DEBUG_CSE
    Zero( A.Matrix() );
}

#ifdef TOM_SAYS_STAY

template<typename T>
void Zero( SparseMatrix<T>& A, bool clearMemory )
{
    EL_DEBUG_CSE
    const Int m = A.Height();
    const Int n = A.Width();
    A.Empty( clearMemory );
    A.Resize( m, n );
}

template<typename T>
void Zero( DistSparseMatrix<T>& A, bool clearMemory )
{
    EL_DEBUG_CSE
    const Int m = A.Height();
    const Int n = A.Width();
    A.Empty( clearMemory );
    A.Resize( m, n );
}

template<typename T>
void Zero( DistMultiVec<T>& X )
{
    EL_DEBUG_CSE
    Zero( X.Matrix() );
}
#endif /* TOM_SAYS_STAY */
#ifdef EL_INSTANTIATE_BLAS_LEVEL1
# define EL_EXTERN
#else
# define EL_EXTERN extern
#endif

#define PROTO(T) \
  EL_EXTERN template void Zero( Matrix<T>& A ); \
  EL_EXTERN template void Zero( AbstractDistMatrix<T>& A );

#ifdef TOM_SAYS_STAY
                                                                        \
  EL_EXTERN template void Zero( SparseMatrix<T>& A, bool clearMemory ); \
  EL_EXTERN template void Zero( DistSparseMatrix<T>& A, bool clearMemory ); \
  EL_EXTERN template void Zero( DistMultiVec<T>& A );
#endif /* TOM_SAYS_STAY */

#define EL_ENABLE_DOUBLEDOUBLE
#define EL_ENABLE_QUADDOUBLE
#define EL_ENABLE_QUAD
#define EL_ENABLE_BIGINT
#define EL_ENABLE_BIGFLOAT
#include <El/macros/Instantiate.h>

#undef EL_EXTERN

} // namespace El

#endif // ifndef EL_BLAS_ZERO_HPP

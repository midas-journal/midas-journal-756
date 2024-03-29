/*======================================================================

  This file is part of the elastix software.

  Copyright (c) University Medical Center Utrecht. All rights reserved.
  See src/CopyrightElastix.txt or http://elastix.isi.uu.nl/legal.php for
  details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE. See the above copyright notices for more information.

======================================================================*/

/** version of original itk file on which code is based: */
/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    $RCSfile: itkBSplineDeformableTransform.txx,v $
  Language:  C++
  Date:      $Date: 2008-05-08 23:22:35 $
  Version:   $Revision: 1.41 $

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __itkAdvancedBSplineDeformableTransform_txx
#define __itkAdvancedBSplineDeformableTransform_txx

#include "itkAdvancedBSplineDeformableTransform.h"
#include "itkContinuousIndex.h"
#include "itkImageRegionIterator.h"
#include "itkImageRegionConstIterator.h"
#include "itkImageRegionConstIteratorWithIndex.h"
#include "itkIdentityTransform.h"
#include "vnl/vnl_math.h"

namespace itk
{

// Constructor with default arguments
template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
AdvancedBSplineDeformableTransform<TScalarType, NDimensions, VSplineOrder>
::AdvancedBSplineDeformableTransform():Superclass()
{
  // Instantiate weights functions
  this->m_WeightsFunction = WeightsFunctionType::New();
  this->m_DerivativeWeightsFunctions.resize( SpaceDimension );
  this->m_SODerivativeWeightsFunctions.resize( SpaceDimension );
  for ( unsigned int i = 0; i < SpaceDimension; ++i )
  {
    this->m_DerivativeWeightsFunctions[ i ] = DerivativeWeightsFunctionType::New();
    this->m_DerivativeWeightsFunctions[ i ]->SetDerivativeDirection( i );
    this->m_SODerivativeWeightsFunctions[ i ].resize( SpaceDimension );
    for ( unsigned int j = 0; j < SpaceDimension; ++j )
    {
      this->m_SODerivativeWeightsFunctions[ i ][ j ] = SODerivativeWeightsFunctionType::New();
      this->m_SODerivativeWeightsFunctions[ i ][ j ]->SetDerivativeDirections( i, j );
    }
  }
  this->m_SupportSize = this->m_WeightsFunction->GetSupportSize();

  // Instantiate an identity transform
  typedef IdentityTransform<ScalarType, SpaceDimension> IdentityTransformType;
  typename IdentityTransformType::Pointer id = IdentityTransformType::New();
  this->m_BulkTransform = id;

  // Default grid size is zero
  typename RegionType::SizeType size;
  typename RegionType::IndexType index;
  size.Fill( 0 );
  index.Fill( 0 );
  this->m_GridRegion.SetSize( size );
  this->m_GridRegion.SetIndex( index );

  this->m_GridOrigin.Fill( 0.0 );  // default origin is all zeros
  this->m_GridSpacing.Fill( 1.0 ); // default spacing is all ones
  this->m_GridDirection.SetIdentity(); // default spacing is all ones
  this->m_GridOffsetTable.Fill( 0 );

  this->m_InternalParametersBuffer = ParametersType(0);
  // Make sure the parameters pointer is not NULL after construction.
  this->m_InputParametersPointer = &(this->m_InternalParametersBuffer);

  // Initialize coeffient images
  for ( unsigned int j = 0; j < SpaceDimension; j++ )
    {
    this->m_WrappedImage[j] = ImageType::New();
    this->m_WrappedImage[j]->SetRegions( this->m_GridRegion );
    this->m_WrappedImage[j]->SetOrigin( this->m_GridOrigin.GetDataPointer() );
    this->m_WrappedImage[j]->SetSpacing( this->m_GridSpacing.GetDataPointer() );
    this->m_WrappedImage[j]->SetDirection( this->m_GridDirection );
    this->m_CoefficientImage[j] = NULL;
    }

  // Setup variables for computing interpolation
  this->m_Offset = SplineOrder / 2;
  if ( SplineOrder % 2 )
    {
    this->m_SplineOrderOdd = true;
    }
  else
    {
    this->m_SplineOrderOdd = false;
    }
  this->m_ValidRegion = this->m_GridRegion;

  // Initialize Jacobian images
  for ( unsigned int j = 0; j < SpaceDimension; j++ )
    {
    this->m_JacobianImage[j] = ImageType::New();
    this->m_JacobianImage[j]->SetRegions( this->m_GridRegion );
    this->m_JacobianImage[j]->SetOrigin( this->m_GridOrigin.GetDataPointer() );
    this->m_JacobianImage[j]->SetSpacing( this->m_GridSpacing.GetDataPointer() );
    this->m_JacobianImage[j]->SetDirection( this->m_GridDirection );
    }

  /** Fixed Parameters store the following information:
   *     Grid Size
   *     Grid Origin
   *     Grid Spacing
   *     Grid Direction
   *  The size of these is equal to the  NInputDimensions
   */
  this->m_FixedParameters.SetSize ( NDimensions * (NDimensions + 3) );
  this->m_FixedParameters.Fill ( 0.0 );
  for (unsigned int i=0; i<NDimensions; i++)
    {
    this->m_FixedParameters[2*NDimensions+i] = this->m_GridSpacing[i];
    }
  for (unsigned int di=0; di<NDimensions; di++)
    {
    for (unsigned int dj=0; dj<NDimensions; dj++)
      {
      this->m_FixedParameters[3*NDimensions+(di*NDimensions+dj)]
        = this->m_GridDirection[di][dj];
      }
    }

  DirectionType scale;
  for( unsigned int i=0; i<SpaceDimension; i++)
    {
    scale[i][i] = this->m_GridSpacing[i];
    }

  this->m_IndexToPoint = this->m_GridDirection * scale;
  this->m_PointToIndexMatrix = this->m_IndexToPoint.GetInverse();
  this->m_PointToIndexMatrixTransposed = this->m_PointToIndexMatrix.GetTranspose();
  this->m_LastJacobianIndex = this->m_ValidRegion.GetIndex();
  for ( unsigned int i = 0; i < SpaceDimension; ++i )
  {
    for ( unsigned int j = 0; j < SpaceDimension; ++j )
    {
      this->m_PointToIndexMatrix2[ i ][ j ]
        = static_cast<ScalarType>( this->m_PointToIndexMatrix[ i ][ j ] );
      this->m_PointToIndexMatrixTransposed2[ i ][ j ]
        = static_cast<ScalarType>( this->m_PointToIndexMatrixTransposed[ i ][ j ] );
    }
  }
}


// Destructor
template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
AdvancedBSplineDeformableTransform<TScalarType, NDimensions, VSplineOrder>
::~AdvancedBSplineDeformableTransform()
{

}


// Set the grid region
template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
void
AdvancedBSplineDeformableTransform<TScalarType, NDimensions, VSplineOrder>
::SetGridRegion( const RegionType& region )
{
  if ( this->m_GridRegion != region )
    {

    this->m_GridRegion = region;

    // set regions for each coefficient and Jacobian image
    for ( unsigned int j = 0; j < SpaceDimension; j++ )
    {
      this->m_WrappedImage[ j ]->SetRegions( this->m_GridRegion );
      this->m_JacobianImage[ j ]->SetRegions( this->m_GridRegion );
    }

    // Set the valid region
    // If the grid spans the interval [start, last].
    // The valid interval for evaluation is [start+offset, last-offset]
    // when spline order is even.
    // The valid interval for evaluation is [start+offset, last-offset)
    // when spline order is odd.
    // Where offset = vcl_floor(spline / 2 ).
    // Note that the last pixel is not included in the valid region
    // with odd spline orders.
    // For backward compatibility m_ValidRegion is still created.
    typename RegionType::SizeType size = this->m_GridRegion.GetSize();
    typename RegionType::IndexType index = this->m_GridRegion.GetIndex();
    typedef typename ContinuousIndexType::ValueType CValueType;
    for ( unsigned int j = 0; j < SpaceDimension; j++ )
    {
      this->m_ValidRegionBegin[ j ] =
        static_cast<CValueType>( index[ j ] ) +
        ( static_cast<CValueType>( SplineOrder ) - 1.0 ) / 2.0;
      this->m_ValidRegionEnd[ j ] =
        static_cast<CValueType>( index[ j ] ) +
        static_cast<CValueType>( size[ j ] - 1 ) -
        ( static_cast<CValueType>( SplineOrder ) - 1.0 ) / 2.0;
      index[ j ] +=
        static_cast< typename RegionType::IndexValueType >( this->m_Offset );
      size[ j ] -=
        static_cast< typename RegionType::SizeValueType> ( 2 * this->m_Offset );
    }
    this->m_ValidRegion.SetSize( size );
    this->m_ValidRegion.SetIndex( index );

    this->UpdateGridOffsetTable();

    //
    // If we are using the default parameters, update their size and set to identity.
    //

    // Input parameters point to internal buffer => using default parameters.
    if (this->m_InputParametersPointer == &(this->m_InternalParametersBuffer) )
      {
      // Check if we need to resize the default parameter buffer.
      if ( this->m_InternalParametersBuffer.GetSize() != this->GetNumberOfParameters() )
        {
        this->m_InternalParametersBuffer.SetSize( this->GetNumberOfParameters() );
        // Fill with zeros for identity.
        this->m_InternalParametersBuffer.Fill( 0 );
        }
      }

    this->Modified();
    }
}


// Transform a point
template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
void
AdvancedBSplineDeformableTransform<TScalarType, NDimensions, VSplineOrder>
::TransformPoint(
  const InputPointType & point,
  OutputPointType & outputPoint,
  WeightsType & weights,
  ParameterIndexArrayType & indices,
  bool & inside ) const
{
  inside = true;

  /** Take care of the initial transform. */
  InputPointType transformedPoint;
  if ( this->m_BulkTransform )
  {
    transformedPoint = this->m_BulkTransform->TransformPoint( point );
  }
  else
  {
    transformedPoint = point;
  }

  /** Check if the coefficient image has been set. */
  if ( !this->m_CoefficientImage[ 0 ] )
  {
    itkWarningMacro( << "B-spline coefficients have not been set" );
    for ( unsigned int j = 0; j < SpaceDimension; j++ )
    {
      outputPoint[ j ] = transformedPoint[ j ];
    }
    return;
  }

  /***/
  ContinuousIndexType cindex;
  this->TransformPointToContinuousGridIndex( point, cindex );

  // NOTE: if the support region does not lie totally within the grid
  // we assume zero displacement and return the input point
  inside = this->InsideValidRegion( cindex );
  if ( !inside )
  {
    outputPoint = transformedPoint;
    return;
  }

  // Compute interpolation weights
  IndexType supportIndex;
  this->m_WeightsFunction->ComputeStartIndex( cindex, supportIndex );
  this->m_WeightsFunction->Evaluate( cindex, supportIndex, weights );

  // For each dimension, correlate coefficient with weights
  RegionType supportRegion;
  supportRegion.SetSize( this->m_SupportSize );
  supportRegion.SetIndex( supportIndex );

  outputPoint.Fill( NumericTraits<ScalarType>::Zero );

  /** Create iterators over the coefficient images. */
  typedef ImageRegionConstIterator<ImageType> IteratorType;
  IteratorType iterator[ SpaceDimension ];
  unsigned long counter = 0;
  const PixelType * basePointer
    = this->m_CoefficientImage[ 0 ]->GetBufferPointer();

  for ( unsigned int j = 0; j < SpaceDimension; j++ )
  {
    iterator[ j ] = IteratorType( this->m_CoefficientImage[ j ], supportRegion );
  }

  /** Loop over the support region. */
  while ( !iterator[ 0 ].IsAtEnd() )
  {
    // populate the indices array
    indices[ counter ] = &(iterator[ 0 ].Value()) - basePointer;

    // multiply weigth with coefficient to compute displacement
    for ( unsigned int j = 0; j < SpaceDimension; j++ )
    {
       outputPoint[ j ] += static_cast<ScalarType>(
         weights[ counter ] * iterator[ j ].Value() );
       ++iterator[ j ];
    }
    ++ counter;

  } // end while

  // The output point is the start point + displacement.
  for ( unsigned int j = 0; j < SpaceDimension; j++ )
  {
    outputPoint[ j ] += transformedPoint[ j ];
  }

}

// Transform a point
template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
typename AdvancedBSplineDeformableTransform<TScalarType, NDimensions, VSplineOrder>
::OutputPointType
AdvancedBSplineDeformableTransform<TScalarType, NDimensions, VSplineOrder>
::TransformPoint(const InputPointType &point) const
{
  /** Allocate memory on the stack: */
  const unsigned long numberOfWeights = WeightsFunctionType::NumberOfWeights;
  typename WeightsType::ValueType weightsArray[ numberOfWeights ];
  typename ParameterIndexArrayType::ValueType indicesArray[ numberOfWeights ];
  WeightsType weights( weightsArray, numberOfWeights, false );
  ParameterIndexArrayType indices( indicesArray, numberOfWeights, false );

  OutputPointType outputPoint;
  bool inside;

  this->TransformPoint( point, outputPoint, weights, indices, inside );

  return outputPoint;
}


// Compute the Jacobian in one position
template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
const
typename AdvancedBSplineDeformableTransform<TScalarType, NDimensions, VSplineOrder>
::JacobianType &
AdvancedBSplineDeformableTransform<TScalarType, NDimensions, VSplineOrder>
::GetJacobian( const InputPointType & point ) const
{
  // Can only compute Jacobian if parameters are set via
  // SetParameters or SetParametersByValue
  if( this->m_InputParametersPointer == NULL )
    {
    itkExceptionMacro( <<"Cannot compute Jacobian: parameters not set" );
    }

  // Zero all components of Jacobian
  // NOTE: for efficiency, we only need to zero out the coefficients
  // that got fill last time this method was called.
  RegionType supportRegion;
  supportRegion.SetSize( this->m_SupportSize );
  supportRegion.SetIndex( this->m_LastJacobianIndex );

  typedef ImageRegionIterator<JacobianImageType> IteratorType;
  IteratorType iterator[ SpaceDimension ];
  unsigned int j;

  for ( j = 0; j < SpaceDimension; j++ )
    {
    iterator[j] = IteratorType( this->m_JacobianImage[j], supportRegion );
    }

  while ( ! iterator[0].IsAtEnd() )
    {

    // zero out Jacobian elements
    for ( j = 0; j < SpaceDimension; j++ )
      {
      iterator[j].Set( NumericTraits<JacobianPixelType>::Zero );
      }

    for ( j = 0; j < SpaceDimension; j++ )
      {
      ++( iterator[j] );
      }
    }


  ContinuousIndexType cindex;

  this->TransformPointToContinuousGridIndex( point, cindex );

  // NOTE: if the support region does not lie totally within the grid
  // we assume zero displacement and return the input point
  if ( !this->InsideValidRegion( cindex ) )
    {
    return this->m_Jacobian;
    }

  // Compute interpolation weights
  IndexType supportIndex;

  /** Allocate memory on the stack: */
  const unsigned long numberOfWeights = WeightsFunctionType::NumberOfWeights;
  typename WeightsType::ValueType weightsArray[ numberOfWeights ];
  WeightsType weights( weightsArray, numberOfWeights, false );

  this->m_WeightsFunction->ComputeStartIndex( cindex, supportIndex );
  this->m_WeightsFunction->Evaluate( cindex, supportIndex, weights );
  this->m_LastJacobianIndex = supportIndex;

  // For each dimension, copy the weight to the support region
  supportRegion.SetIndex( supportIndex );
  unsigned long counter = 0;

  for ( j = 0; j < SpaceDimension; j++ )
    {
    iterator[j] = IteratorType( this->m_JacobianImage[j], supportRegion );
    }

  while ( !iterator[0].IsAtEnd() )
    {

    // copy weight to Jacobian image
    for ( j = 0; j < SpaceDimension; j++ )
      {
      iterator[j].Set( static_cast<JacobianPixelType>( weights[counter] ) );
      }

    // go to next coefficient in the support region
    ++ counter;
    for ( j = 0; j < SpaceDimension; j++ )
      {
      ++( iterator[j] );
      }
    }

  // Return the results
  return this->m_Jacobian;

}


// Compute the Jacobian in one position
template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
void
AdvancedBSplineDeformableTransform<TScalarType, NDimensions,VSplineOrder>
::GetJacobian( const InputPointType & point, WeightsType& weights,
  ParameterIndexArrayType & indexes ) const
{

  RegionType supportRegion;
  supportRegion.SetSize( this->m_SupportSize );
  const PixelType * basePointer = this->m_CoefficientImage[0]->GetBufferPointer();

  ContinuousIndexType cindex;

  this->TransformPointToContinuousGridIndex( point, cindex );

  // NOTE: if the support region does not lie totally within the grid
  // we assume zero displacement and return the input point
  if ( !this->InsideValidRegion( cindex ) )
    {
    weights.Fill(0.0);
    indexes.Fill(0);
    return;
    }

  // Compute interpolation weights
  IndexType supportIndex;

  this->m_WeightsFunction->ComputeStartIndex( cindex, supportIndex );
  this->m_WeightsFunction->Evaluate( cindex, supportIndex, weights );

  // For each dimension, copy the weight to the support region
  supportRegion.SetIndex( supportIndex );
  unsigned long counter = 0;

  typedef ImageRegionIterator<JacobianImageType> IteratorType;

  IteratorType iterator = IteratorType( this->m_CoefficientImage[0], supportRegion );

  while ( ! iterator.IsAtEnd() )
  {
    indexes[counter] = &(iterator.Value()) - basePointer;

    // go to next coefficient in the support region
    ++ counter;
    ++iterator;
  }

}


template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
unsigned int
AdvancedBSplineDeformableTransform<TScalarType, NDimensions,VSplineOrder>
::GetNumberOfAffectedWeights() const
{
  return this->m_WeightsFunction->GetNumberOfWeights();
}


/**
 * ********************* GetNumberOfNonZeroJacobianIndices ****************************
 */

template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
unsigned long
AdvancedBSplineDeformableTransform<TScalarType, NDimensions,VSplineOrder>
::GetNumberOfNonZeroJacobianIndices( void ) const
{
  return this->m_WeightsFunction->GetNumberOfWeights() * SpaceDimension;

} // end GetNumberOfNonZeroJacobianIndices()


/**
 * ********************* GetJacobian ****************************
 */

template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
void
AdvancedBSplineDeformableTransform<TScalarType, NDimensions,VSplineOrder>
::GetJacobian(
  const InputPointType & ipp,
  JacobianType & jacobian,
  NonZeroJacobianIndicesType & nonZeroJacobianIndices ) const
{
  /** This implements a sparse version of the Jacobian. */
  // Can only compute Jacobian if parameters are set via
  // SetParameters or SetParametersByValue
  if ( this->m_InputParametersPointer == NULL )
  {
    itkExceptionMacro( << "Cannot compute Jacobian: parameters not set" );
  }

  /** Convert the physical point to a continuous index, which
   * is needed for the 'Evaluate()' functions below.
   */
  ContinuousIndexType cindex;
  this->TransformPointToContinuousGridIndex( ipp, cindex );

  /** Initialize */
  const unsigned int nnzji = this->GetNumberOfNonZeroJacobianIndices();
  if ( (jacobian.cols() != nnzji) || (jacobian.rows() != SpaceDimension) )
  {
    jacobian.SetSize( SpaceDimension, nnzji );
  }
  jacobian.Fill( 0.0 );

  // NOTE: if the support region does not lie totally within the grid
  // we assume zero displacement and zero Jacobian
  if ( !this->InsideValidRegion( cindex ) )
  {
    /** Return some dummy */
    nonZeroJacobianIndices.resize( this->GetNumberOfNonZeroJacobianIndices() );
    for (unsigned int i = 0; i < this->GetNumberOfNonZeroJacobianIndices(); ++i )
    {
      nonZeroJacobianIndices[i] = i;
    }
    return;
  }

  /** Helper variables. */
  IndexType supportIndex;

  /** Compute the number of affected B-spline parameters. */
  /** Allocate memory on the stack: */
  const unsigned long numberOfWeights = WeightsFunctionType::NumberOfWeights;
  typename WeightsType::ValueType weightsArray[ numberOfWeights ];
  WeightsType weights( weightsArray, numberOfWeights, false );

  /** Compute the derivative weights. */
  this->m_WeightsFunction->ComputeStartIndex( cindex, supportIndex );
  this->m_WeightsFunction->Evaluate( cindex, supportIndex, weights );

  /** Set up support region */
  RegionType supportRegion;
  supportRegion.SetSize( this->m_SupportSize );
  supportRegion.SetIndex( supportIndex );

  /** Put at the right positions */
  unsigned int counter = 0;
  for ( unsigned int d = 0; d < SpaceDimension; ++d )
  {
    for ( unsigned int mu = 0; mu < numberOfWeights; ++mu )
    {
      jacobian( d, counter ) = weights[ mu ];
      ++counter;
    }
  }

  /** Compute the nonzero Jacobian indices. */
  this->ComputeNonZeroJacobianIndices( nonZeroJacobianIndices, supportRegion );

} // end GetJacobian()


/**
 * ********************* GetSpatialJacobian ****************************
 */

template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
void
AdvancedBSplineDeformableTransform<TScalarType, NDimensions,VSplineOrder>
::GetSpatialJacobian(
  const InputPointType & ipp,
  SpatialJacobianType & sj ) const
{
  // Can only compute Jacobian if parameters are set via
  // SetParameters or SetParametersByValue
  if ( this->m_InputParametersPointer == NULL )
  {
    itkExceptionMacro( << "Cannot compute Jacobian: parameters not set" );
  }

  /** Convert the physical point to a continuous index, which
   * is needed for the 'Evaluate()' functions below.
   */
  ContinuousIndexType cindex;
  this->TransformPointToContinuousGridIndex( ipp, cindex );

  // NOTE: if the support region does not lie totally within the grid
  // we assume zero displacement and identity spatial Jacobian
  if ( !this->InsideValidRegion( cindex ) )
  {
    sj.SetIdentity();
    return;
  }

  /** Compute the number of affected B-spline parameters. */
  /** Allocate memory on the stack: */
  const unsigned long numberOfWeights = WeightsFunctionType::NumberOfWeights;
  typename WeightsType::ValueType weightsArray[ numberOfWeights ];
  WeightsType weights( weightsArray, numberOfWeights, false );

  IndexType supportIndex;
  this->m_DerivativeWeightsFunctions[ 0 ]->ComputeStartIndex(
    cindex, supportIndex );
  RegionType supportRegion;
  supportRegion.SetSize( this->m_SupportSize );
  supportRegion.SetIndex( supportIndex );

  /** Compute the spatial Jacobian sj:
   *    dT_{dim} / dx_i = delta_{dim,i} + \sum coefs_{dim} * weights * PointToGridIndex.
   */
  sj.Fill( 0.0 );
  for ( unsigned int i = 0; i < SpaceDimension; ++i )
  {
    /** Compute the derivative weights. */
    this->m_DerivativeWeightsFunctions[ i ]->Evaluate( cindex, supportIndex, weights );

    /** Compute the spatial Jacobian sj:
     *    dT_{dim} / dx_i = \sum coefs_{dim} * weights.
     */
    for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
    {
      /** Create an iterator over the correct part of the coefficient
       * image. Create an iterator over the weights vector.
       */
      ImageRegionConstIterator<ImageType> itCoef(
        this->m_CoefficientImage[ dim ], supportRegion );
      typename WeightsType::const_iterator itWeights = weights.begin();

      /** Compute the sum for this dimension. */
      double sum = 0.0;
      while ( !itCoef.IsAtEnd() )
      {
        sum += itCoef.Value() * (*itWeights);
        ++itWeights;
        ++itCoef;
      }

      /** Update the spatial Jacobian sj. */
      sj( dim, i ) += sum;

    } // end for dim
  } // end for i

  /** Take into account grid spacing and direction cosines. */
  sj = sj * this->m_PointToIndexMatrix2;

  /** Add contribution of spatial derivative of x. */
  for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
  {
    sj( dim, dim ) += 1.0;
  }

} // end GetSpatialJacobian()


/**
 * ********************* GetSpatialHessian ****************************
 */

template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
void
AdvancedBSplineDeformableTransform<TScalarType, NDimensions,VSplineOrder>
::GetSpatialHessian(
  const InputPointType & ipp,
  SpatialHessianType & sh ) const
{
  // Can only compute Jacobian if parameters are set via
  // SetParameters or SetParametersByValue
  if ( this->m_InputParametersPointer == NULL )
  {
    itkExceptionMacro( << "Cannot compute Jacobian: parameters not set" );
  }

  /** Convert the physical point to a continuous index, which
   * is needed for the evaluate functions below.
   */
  ContinuousIndexType cindex;
  this->TransformPointToContinuousGridIndex( ipp, cindex );

  // NOTE: if the support region does not lie totally within the grid
  // we assume zero displacement and zero spatial Hessian
  if ( !this->InsideValidRegion( cindex ) )
  {
    for ( unsigned int i = 0; i < sh.Size(); ++i )
    {
      sh[ i ].Fill( 0.0 );
    }
    return;
  }

  /** Helper variables. */
  /** Allocate memory on the stack: */
  const unsigned long numberOfWeights = WeightsFunctionType::NumberOfWeights;
  typename WeightsType::ValueType weightsArray[ numberOfWeights ];
  WeightsType weights( weightsArray, numberOfWeights, false );

  IndexType supportIndex;
  this->m_SODerivativeWeightsFunctions[ 0 ][ 0 ]->ComputeStartIndex(
    cindex, supportIndex );
  RegionType supportRegion;
  supportRegion.SetSize( this->m_SupportSize );
  supportRegion.SetIndex( supportIndex );

  /** For all derivative directions, compute the spatial Hessian.
   * The derivatives are d^2T / dx_i dx_j.
   * Make use of the fact that the Hessian is symmetrical, so do not compute
   * both i,j and j,i for i != j.
   */
  for ( unsigned int i = 0; i < SpaceDimension; ++i )
  {
    for ( unsigned int j = 0; j <= i; ++j )
    {
      /** Compute the derivative weights. */
      this->m_SODerivativeWeightsFunctions[ i ][ j ]
        ->Evaluate( cindex, supportIndex, weights );

      /** Compute d^2T_{dim} / dx_i dx_j = \sum coefs_{dim} * weights. */
      for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
      {
        /** Create an iterator over the correct part of the coefficient image. */
        ImageRegionConstIterator<ImageType> itCoef(
          this->m_CoefficientImage[ dim ], supportRegion );

        /** Compute the sum for this dimension. */
        double sum = 0.0;
        unsigned int mu = 0;
        while ( !itCoef.IsAtEnd() )
        {
          sum += itCoef.Value() * weights[ mu ];
          ++itCoef;
          ++mu;
        }

        /** Update the spatial Hessian sh. The Hessian is symmetrical. */
        sh[ dim ][ i ][ j ] = sum;
        sh[ dim ][ j ][ i ] = sum;
      }
    }
  }

  /** Take into account grid spacing and direction matrix */
  for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
  {
    sh[dim] = this->m_PointToIndexMatrixTransposed2
      * ( sh[dim] * this->m_PointToIndexMatrix2 );
  }

} // end GetSpatialHessian()


/**
 * ********************* GetJacobianOfSpatialJacobian ****************************
 */

template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
void
AdvancedBSplineDeformableTransform<TScalarType, NDimensions,VSplineOrder>
::GetJacobianOfSpatialJacobian(
  const InputPointType & ipp,
  JacobianOfSpatialJacobianType & jsj,
  NonZeroJacobianIndicesType & nonZeroJacobianIndices ) const
{
  // Can only compute Jacobian if parameters are set via
  // SetParameters or SetParametersByValue
  if ( this->m_InputParametersPointer == NULL )
  {
    itkExceptionMacro( << "Cannot compute Jacobian: parameters not set" );
  }

  jsj.resize( this->GetNumberOfNonZeroJacobianIndices() );

  /** Convert the physical point to a continuous index, which
   * is needed for the 'Evaluate()' functions below.
   */
  ContinuousIndexType cindex;
  this->TransformPointToContinuousGridIndex( ipp, cindex );

  // NOTE: if the support region does not lie totally within the grid
  // we assume zero displacement and zero jsj.
  if ( !this->InsideValidRegion( cindex ) )
  {
    for ( unsigned int i = 0; i < jsj.size(); ++i )
    {
      jsj[ i ].Fill( 0.0 );
    }
    nonZeroJacobianIndices.resize( this->GetNumberOfNonZeroJacobianIndices() );
    for ( unsigned int i = 0; i < this->GetNumberOfNonZeroJacobianIndices(); ++i )
    {
      nonZeroJacobianIndices[ i ] = i;
    }
    return;
  }

  /** Helper variables. */

  /** Allocate memory on the stack: */
  const unsigned long numberOfWeights = WeightsFunctionType::NumberOfWeights;
  typename WeightsType::ValueType weightsArray[ numberOfWeights ];
  WeightsType weights( weightsArray, numberOfWeights, false );

  IndexType supportIndex;
  this->m_DerivativeWeightsFunctions[ 0 ]->ComputeStartIndex(
    cindex, supportIndex );
  RegionType supportRegion;
  supportRegion.SetSize( this->m_SupportSize );
  supportRegion.SetIndex( supportIndex );

  /** On the stack instead of heap is faster. */
  double weightVector[ SpaceDimension * numberOfWeights ];

  /** For all derivative directions, compute the derivatives of the
   * spatial Jacobian to the transformation parameters mu:
   * d/dmu of dT / dx_i
   */
  for ( unsigned int i = 0; i < SpaceDimension; ++i )
  {
    /** Compute the derivative weights. */
    this->m_DerivativeWeightsFunctions[ i ]->Evaluate( cindex, supportIndex, weights );

    /** Remember the weights. */
    memcpy( weightVector + i * numberOfWeights,
      weights.data_block(), numberOfWeights * sizeof( double ) );

  } // end for i

  /** Compute the Jacobian of the spatial Jacobian jsj:
   *    d/dmu dT_{dim} / dx_i = weights.
   */
  SpatialJacobianType * basepointer = &jsj[ 0 ];
  for ( unsigned int mu = 0; mu < numberOfWeights; ++mu )
  {
    for ( unsigned int i = 0; i < SpaceDimension; ++i )
    {
      double tmp = *( weightVector + i * numberOfWeights + mu );
      for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
      {
        (*( basepointer + dim * numberOfWeights + mu ))( dim, i ) = tmp;
      }
    }
  }

  /** Take into account grid spacing and direction cosines */
  for ( unsigned int i = 0; i < jsj.size(); ++i )
  {
    jsj[ i ] = jsj[ i ] * this->m_PointToIndexMatrix2;
  }

  /** Compute the nonzero Jacobian indices. */
  this->ComputeNonZeroJacobianIndices( nonZeroJacobianIndices, supportRegion );

} // end GetJacobianOfSpatialJacobian()


/**
 * ********************* GetJacobianOfSpatialJacobian ****************************
 */

template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
void
AdvancedBSplineDeformableTransform<TScalarType, NDimensions,VSplineOrder>
::GetJacobianOfSpatialJacobian(
  const InputPointType & ipp,
  SpatialJacobianType & sj,
  JacobianOfSpatialJacobianType & jsj,
  NonZeroJacobianIndicesType & nonZeroJacobianIndices ) const
{
  // Can only compute Jacobian if parameters are set via
  // SetParameters or SetParametersByValue
  if ( this->m_InputParametersPointer == NULL )
  {
    itkExceptionMacro( << "Cannot compute Jacobian: parameters not set" );
  }

  jsj.resize( this->GetNumberOfNonZeroJacobianIndices() );

  /** Convert the physical point to a continuous index, which
   * is needed for the 'Evaluate()' functions below.
   */
  ContinuousIndexType cindex;
  this->TransformPointToContinuousGridIndex( ipp, cindex );

  // NOTE: if the support region does not lie totally within the grid
  // we assume zero displacement and identity sj and zero jsj.
  if ( !this->InsideValidRegion( cindex ) )
  {
    sj.SetIdentity();
    for ( unsigned int i = 0; i < jsj.size(); ++i )
    {
      jsj[ i ].Fill( 0.0 );
    }
    nonZeroJacobianIndices.resize( this->GetNumberOfNonZeroJacobianIndices() );
    for ( unsigned int i = 0; i < this->GetNumberOfNonZeroJacobianIndices(); ++i )
    {
      nonZeroJacobianIndices[ i ] = i;
    }
    return;
  }

  /** Compute the number of affected B-spline parameters. */

  /** Allocate memory on the stack: */
  const unsigned long numberOfWeights = WeightsFunctionType::NumberOfWeights;
  typename WeightsType::ValueType weightsArray[ numberOfWeights ];
  WeightsType weights( weightsArray, numberOfWeights, false );

  /** Helper variables. */
  IndexType supportIndex;
  this->m_DerivativeWeightsFunctions[ 0 ]->ComputeStartIndex(
    cindex, supportIndex );
  RegionType supportRegion;
  supportRegion.SetSize( this->m_SupportSize );
  supportRegion.SetIndex( supportIndex );

  /** On the stack instead of heap is faster. */
  double weightVector[ SpaceDimension * numberOfWeights ];

  /** Initialize the spatial Jacobian sj: */
  sj.Fill( 0.0 );

  /** For all derivative directions, compute the derivatives of the
   * spatial Jacobian to the transformation parameters mu: d/dmu of dT / dx_i
   */
  for ( unsigned int i = 0; i < SpaceDimension; ++i )
  {
    /** Compute the derivative weights. */
    this->m_DerivativeWeightsFunctions[ i ]->Evaluate( cindex, supportIndex, weights );
    /** \todo: we can realise some speedup here to compute the derivative
     * weights at once for all dimensions */

    /** Remember the weights. */
    memcpy( weightVector + i * numberOfWeights,
      weights.data_block(), numberOfWeights * sizeof( double ) );

    /** Compute the spatial Jacobian sj:
     *    dT_{dim} / dx_i = delta_{dim,i} + \sum coefs_{dim} * weights.
     */
    for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
    {
      /** Create an iterator over the correct part of the coefficient
       * image. Create an iterator over the weights vector.
       */
      ImageRegionConstIterator<ImageType> itCoef(
        this->m_CoefficientImage[ dim ], supportRegion );
      typename WeightsType::const_iterator itWeights = weights.begin();

      /** Compute the sum for this dimension. */
      double sum = 0.0;
      while ( !itCoef.IsAtEnd() )
      {
        sum += itCoef.Value() * (*itWeights);
        ++itWeights;
        ++itCoef;
      }

      /** Update the spatial Jacobian sj. */
      sj( dim, i ) += sum;

    } // end for dim
  } // end for i

  /** Take into account grid spacing and direction cosines. */
  sj = sj * this->m_PointToIndexMatrix2;

  /** Add contribution of spatial derivative of x. */
  for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
  {
    sj( dim, dim ) += 1.0;
  }

  /** Compute the Jacobian of the spatial Jacobian jsj:
   *    d/dmu dT_{dim} / dx_i = weights.
   */
  SpatialJacobianType * basepointer = &jsj[ 0 ];
  for ( unsigned int mu = 0; mu < numberOfWeights; ++mu )
  {
    for ( unsigned int i = 0; i < SpaceDimension; ++i )
    {
      double tmp = *( weightVector + i * numberOfWeights + mu );
      for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
      {
        (*( basepointer + dim * numberOfWeights + mu ))( dim, i ) = tmp;
      }
    }
  }

  /** Take into account grid spacing and direction cosines */
  for ( unsigned int i = 0; i < jsj.size(); ++i )
  {
    jsj[ i ] = jsj[ i ] * this->m_PointToIndexMatrix2;
  }

  /** Compute the nonzero Jacobian indices. */
  this->ComputeNonZeroJacobianIndices( nonZeroJacobianIndices, supportRegion );

} // end GetJacobianOfSpatialJacobian()


/**
 * ********************* GetJacobianOfSpatialHessian ****************************
 */

template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
void
AdvancedBSplineDeformableTransform<TScalarType, NDimensions,VSplineOrder>
::GetJacobianOfSpatialHessian(
  const InputPointType & ipp,
  JacobianOfSpatialHessianType & jsh,
  NonZeroJacobianIndicesType & nonZeroJacobianIndices ) const
{
  // Can only compute Jacobian if parameters are set via
  // SetParameters or SetParametersByValue
  if ( this->m_InputParametersPointer == NULL )
  {
    itkExceptionMacro( << "Cannot compute Jacobian: parameters not set" );
  }

  jsh.resize( this->GetNumberOfNonZeroJacobianIndices() );

  /** Convert the physical point to a continuous index, which
   * is needed for the 'Evaluate()' functions below.
   */
  ContinuousIndexType cindex;
  this->TransformPointToContinuousGridIndex( ipp, cindex );

  // NOTE: if the support region does not lie totally within the grid
  // we assume zero displacement and identity sj and zero jsj.
  if ( !this->InsideValidRegion( cindex ) )
  {
    for ( unsigned int i = 0; i < jsh.size(); ++i )
    {
      for ( unsigned int j = 0; j < jsh[ i ].Size(); ++j )
      {
        jsh[ i ][ j ].Fill( 0.0 );
      }
    }
    nonZeroJacobianIndices.resize( this->GetNumberOfNonZeroJacobianIndices() );
    for ( unsigned int i = 0; i < this->GetNumberOfNonZeroJacobianIndices(); ++i )
    {
      nonZeroJacobianIndices[ i ] = i;
    }
    return;
  }

  /** Compute the number of affected B-spline parameters. */

  /** Allocate memory on the stack: */
  const unsigned long numberOfWeights = WeightsFunctionType::NumberOfWeights;
  typename WeightsType::ValueType weightsArray[ numberOfWeights ];
  WeightsType weights( weightsArray, numberOfWeights, false );

  IndexType supportIndex;
  this->m_SODerivativeWeightsFunctions[ 0 ][ 0 ]->ComputeStartIndex(
    cindex, supportIndex );
  RegionType supportRegion;
  supportRegion.SetSize( this->m_SupportSize );
  supportRegion.SetIndex( supportIndex );

  /** For all derivative directions, compute the derivatives of the
   * spatial Hessian to the transformation parameters mu:
   * d/dmu of d^2T / dx_i dx_j
   * Make use of the fact that the Hessian is symmetrical, so do not compute
   * both i,j and j,i for i != j.
   */
  const unsigned int d = SpaceDimension * ( SpaceDimension + 1 ) / 2;
  FixedArray<WeightsType, d> weightVector;
  unsigned int count = 0;
  for ( unsigned int i = 0; i < SpaceDimension; ++i )
  {
    for ( unsigned int j = 0; j <= i; ++j )
    {
      /** Compute the derivative weights. */
      this->m_SODerivativeWeightsFunctions[ i ][ j ]->Evaluate( cindex, supportIndex, weights );

      /** Remember the weights. */
      weightVector[ count ] = weights;
      ++count;

    } // end for j
  } // end for i

  /** Compute d/dmu d^2T_{dim} / dx_i dx_j = weights. */
  SpatialHessianType * basepointer = &jsh[ 0 ];
  for ( unsigned int mu = 0; mu < numberOfWeights; ++mu )
  {
    SpatialJacobianType matrix;
    unsigned int count = 0;
    for ( unsigned int i = 0; i < SpaceDimension; ++i )
    {
      for ( unsigned int j = 0; j <= i; ++j )
      {
        double tmp = weightVector[ count ][ mu ];
        matrix[ i ][ j ] = tmp;
        if ( i != j ) matrix[ j ][ i ] = tmp;
        ++count;
      }
    }

    for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
    {
      (*(basepointer + mu + dim * numberOfWeights))[ dim ] = matrix;
    }
  }

  /** Take into account grid spacing and direction matrix */
  for ( unsigned int i = 0; i < jsh.size(); ++i )
  {
    for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
    {
      jsh[i][dim] = this->m_PointToIndexMatrixTransposed2
        * ( jsh[i][dim] * this->m_PointToIndexMatrix2 );
    }
  }

  /** Compute the nonzero Jacobian indices. */
  this->ComputeNonZeroJacobianIndices( nonZeroJacobianIndices, supportRegion );

} // end GetJacobianOfSpatialHessian()


/**
 * ********************* GetJacobianOfSpatialHessian ****************************
 */

template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
void
AdvancedBSplineDeformableTransform<TScalarType, NDimensions,VSplineOrder>
::GetJacobianOfSpatialHessian(
  const InputPointType & ipp,
  SpatialHessianType & sh,
  JacobianOfSpatialHessianType & jsh,
  NonZeroJacobianIndicesType & nonZeroJacobianIndices ) const
{
  // Can only compute Jacobian if parameters are set via
  // SetParameters or SetParametersByValue
  if ( this->m_InputParametersPointer == NULL )
  {
    itkExceptionMacro( << "Cannot compute Jacobian: parameters not set" );
  }

  jsh.resize( this->GetNumberOfNonZeroJacobianIndices() );

  /** Convert the physical point to a continuous index, which
   * is needed for the 'Evaluate()' functions below.
   */
  ContinuousIndexType cindex;
  this->TransformPointToContinuousGridIndex( ipp, cindex );

  // NOTE: if the support region does not lie totally within the grid
  // we assume zero displacement and identity sj and zero jsj.
  if ( !this->InsideValidRegion( cindex ) )
  {
    for ( unsigned int i = 0; i < jsh.size(); ++i )
    {
      for ( unsigned int j = 0; j < jsh[ i ].Size(); ++j )
      {
        jsh[ i ][ j ].Fill( 0.0 );
      }
    }
    for ( unsigned int i = 0; i < sh.Size(); ++i )
    {
      sh[ i ].Fill( 0.0 );
    }
    nonZeroJacobianIndices.resize( this->GetNumberOfNonZeroJacobianIndices() );
    for ( unsigned int i = 0; i < this->GetNumberOfNonZeroJacobianIndices(); ++i )
    {
      nonZeroJacobianIndices[ i ] = i;
    }
    return;
  }

  /** Compute the number of affected B-spline parameters. */

  /** Allocate memory on the stack: */
  const unsigned long numberOfWeights = WeightsFunctionType::NumberOfWeights;
  typename WeightsType::ValueType weightsArray[ numberOfWeights ];
  WeightsType weights( weightsArray, numberOfWeights, false );

  IndexType supportIndex;
  this->m_SODerivativeWeightsFunctions[ 0 ][ 0 ]->ComputeStartIndex(
    cindex, supportIndex );
  RegionType supportRegion;
  supportRegion.SetSize( this->m_SupportSize );
  supportRegion.SetIndex( supportIndex );

  /** On the stack instead of heap is faster. */
  const unsigned int d = SpaceDimension * ( SpaceDimension + 1 ) / 2;
  double weightVector[ d * numberOfWeights ];

  /** For all derivative directions, compute the derivatives of the
   * spatial Hessian to the transformation parameters mu:
   * d/dmu of d^2T / dx_i dx_j
   * Make use of the fact that the Hessian is symmetrical, so do not compute
   * both i,j and j,i for i != j.
   */
  unsigned int count = 0;
  for ( unsigned int i = 0; i < SpaceDimension; ++i )
  {
    for ( unsigned int j = 0; j <= i; ++j )
    {
      /** Compute the derivative weights. */
      this->m_SODerivativeWeightsFunctions[ i ][ j ]
        ->Evaluate( cindex, supportIndex, weights );

      /** Remember the weights. */
      memcpy( weightVector + count * numberOfWeights,
        weights.data_block(), numberOfWeights * sizeof( double ) );
      count++;

      /** Compute the spatial Hessian sh:
       *    d^2T_{dim} / dx_i dx_j = \sum coefs_{dim} * weights.
       */
      for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
      {
        /** Create an iterator over the correct part of the coefficient
         * image. Create an iterator over the weights vector.
         */
        ImageRegionConstIterator<ImageType> itCoef(
          this->m_CoefficientImage[ dim ], supportRegion );
        typename WeightsType::const_iterator itWeights = weights.begin();

        /** Compute the sum for this dimension. */
        double sum = 0.0;
        while ( !itCoef.IsAtEnd() )
        {
          sum += itCoef.Value() * (*itWeights);
          ++itWeights;
          ++itCoef;
        }

        /** Update the spatial Hessian sh. The Hessian is symmetrical. */
        sh[ dim ]( i, j ) = sum;
        sh[ dim ]( j, i ) = sum;
      }

    } // end for j
  } // end for i

  /** Compute the Jacobian of the spatial Hessian jsh:
   *    d/dmu d^2T_{dim} / dx_i dx_j = weights.
   */
  SpatialHessianType * basepointer = &jsh[ 0 ];
  for ( unsigned int mu = 0; mu < numberOfWeights; ++mu )
  {
    SpatialJacobianType matrix;
    unsigned int count = 0;
    for ( unsigned int i = 0; i < SpaceDimension; ++i )
    {
      for ( unsigned int j = 0; j <= i; ++j )
      {
        double tmp = *( weightVector + count * numberOfWeights + mu );
        //double tmp = weightVector[ count ][ mu ];
        matrix[ i ][ j ] = tmp;
        if ( i != j ) matrix[ j ][ i ] = tmp;
        ++count;
      }
    }

    for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
    {
      (*(basepointer + mu + dim * numberOfWeights))[ dim ] = matrix;
    }
  }

  /** Take into account grid spacing and direction matrix */
  for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
  {
    sh[dim] = this->m_PointToIndexMatrixTransposed2
      * ( sh[dim] * this->m_PointToIndexMatrix2 );
  }
  for ( unsigned int i = 0; i < jsh.size(); ++i )
  {
    for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
    {
      jsh[i][dim] = this->m_PointToIndexMatrixTransposed2
        * ( jsh[i][dim] * this->m_PointToIndexMatrix2 );
    }
  }

  /** Compute the nonzero Jacobian indices. */
  this->ComputeNonZeroJacobianIndices( nonZeroJacobianIndices, supportRegion );

} // end GetJacobianOfSpatialHessian()


/**
 * ********************* ComputeNonZeroJacobianIndices ****************************
 */

template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
void
AdvancedBSplineDeformableTransform<TScalarType, NDimensions,VSplineOrder>
::ComputeNonZeroJacobianIndices(
  NonZeroJacobianIndicesType & nonZeroJacobianIndices,
  const RegionType & supportRegion ) const
{
  nonZeroJacobianIndices.resize( this->GetNumberOfNonZeroJacobianIndices() );

  /** Create an iterator over the coefficient image. */
  ImageRegionConstIterator< ImageType >
    it( this->m_CoefficientImage[ 0 ], supportRegion );

  /** Initialize some helper variables. */
  const unsigned long numberOfWeights = WeightsFunctionType::NumberOfWeights;
  const unsigned long parametersPerDim
    = this->GetNumberOfParametersPerDimension();
  unsigned long mu = 0;

  /** For all control points in the support region, set which of the
   * indices in the parameter array are non-zero.
   */
  const PixelType * basePointer = this->m_CoefficientImage[0]->GetBufferPointer();

  while ( !it.IsAtEnd() )
  {
    unsigned long parameterNumber = &(it.Value()) - basePointer;

    /** Update the nonZeroJacobianIndices for all directions. */
    for ( unsigned int dim = 0; dim < SpaceDimension; ++dim )
    {
      nonZeroJacobianIndices[ mu + dim * numberOfWeights ]
        = parameterNumber + dim * parametersPerDim;
    }

    /** Increase the iterators. */
    ++it;
    ++mu;

  } // end while

} // end ComputeNonZeroJacobianIndices()

// Print self
template<class TScalarType, unsigned int NDimensions, unsigned int VSplineOrder>
void
AdvancedBSplineDeformableTransform<TScalarType, NDimensions, VSplineOrder>
::PrintSelf(std::ostream &os, Indent indent) const
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "WeightsFunction: ";
  os << this->m_WeightsFunction.GetPointer() << std::endl;
}


} // namespace

#endif

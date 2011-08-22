/*======================================================================

  This file is part of the elastix software.

  Copyright (c) University Medical Center Utrecht. All rights reserved.
  See src/CopyrightElastix.txt or http://elastix.isi.uu.nl/legal.php for
  details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE. See the above copyright notices for more information.

======================================================================*/
#ifndef __itkTransformBendingEnergyPenaltyTerm_txx
#define __itkTransformBendingEnergyPenaltyTerm_txx

#include "itkTransformBendingEnergyPenaltyTerm.h"


namespace itk
{

/**
 * ****************** Constructor *******************************
 */

template< class TFixedImage, class TScalarType >
TransformBendingEnergyPenaltyTerm< TFixedImage, TScalarType >
::TransformBendingEnergyPenaltyTerm()
{
  /** Initialize member variables. */

  /** Turn on the sampler functionality. */
  this->SetUseImageSampler( true );

} // end constructor


/**
 * ****************** GetValue *******************************
 */

template< class TFixedImage, class TScalarType >
typename TransformBendingEnergyPenaltyTerm< TFixedImage, TScalarType >::MeasureType
TransformBendingEnergyPenaltyTerm< TFixedImage, TScalarType >
::GetValue( const ParametersType & parameters ) const
{
  /** Initialize some variables. */
  this->m_NumberOfPixelsCounted = 0;
  RealType measure = NumericTraits<RealType>::Zero;
  SpatialHessianType spatialHessian;

  /** Check if the SpatialHessian is nonzero. */
  if ( !this->m_AdvancedTransform->GetHasNonZeroSpatialHessian() )
  {
    return static_cast<MeasureType>( measure );
  }

  /** Make sure the transform parameters are up to date. */
  this->SetTransformParameters( parameters );

  /** Update the imageSampler and get a handle to the sample container. */
  this->GetImageSampler()->Update();
  ImageSampleContainerPointer sampleContainer = this->GetImageSampler()->GetOutput();

  /** Create iterator over the sample container. */
  typename ImageSampleContainerType::ConstIterator fiter;
  typename ImageSampleContainerType::ConstIterator fbegin = sampleContainer->Begin();
  typename ImageSampleContainerType::ConstIterator fend = sampleContainer->End();

  /** Loop over the fixed image samples to calculate the penalty term. */
  for ( fiter = fbegin; fiter != fend; ++fiter )
  {
    /** Read fixed coordinates and initialize some variables. */
    const FixedImagePointType & fixedPoint = (*fiter).Value().m_ImageCoordinates;
    MovingImagePointType mappedPoint;

    /** Transform point and check if it is inside the B-spline support region. */
    bool sampleOk = this->TransformPoint( fixedPoint, mappedPoint );

    /** Check if point is inside mask. */
    if ( sampleOk )
    {
      sampleOk = this->IsInsideMovingMask( mappedPoint );
    }

    if ( sampleOk )
    {
      this->m_NumberOfPixelsCounted++;

      /** Get the spatial Hessian of the transformation at the current point.
       * This is needed to compute the bending energy.
       */
      this->m_AdvancedTransform->GetSpatialHessian( fixedPoint, spatialHessian );

      /** Compute the contribution of this point. */
      for ( unsigned int k = 0; k < FixedImageDimension; ++k )
      {
        measure += vnl_math_sqr(
          spatialHessian[ k ].GetVnlMatrix().frobenius_norm() );
      }

    } // end if sampleOk

  } // end for loop over the image sample container

  /** Check if enough samples were valid. */
  this->CheckNumberOfSamples(
    sampleContainer->Size(), this->m_NumberOfPixelsCounted );

  /** Update measure value. */
  measure /= static_cast<RealType>( this->m_NumberOfPixelsCounted );

  /** Return the value. */
  return static_cast<MeasureType>( measure );

} // end GetValue()


/**
 * ******************* GetDerivative *******************
 */

template< class TFixedImage, class TScalarType >
void
TransformBendingEnergyPenaltyTerm< TFixedImage, TScalarType >
::GetDerivative(
  const ParametersType & parameters,
  DerivativeType & derivative ) const
{
  /** Slower, but works. */
  MeasureType dummyvalue = NumericTraits< MeasureType >::Zero;
  this->GetValueAndDerivative( parameters, dummyvalue, derivative );

} // end GetDerivative()


/**
 * ****************** GetValueAndDerivative *******************************
 */

template< class TFixedImage, class TScalarType >
void
TransformBendingEnergyPenaltyTerm< TFixedImage, TScalarType >
::GetValueAndDerivative(
  const ParametersType & parameters,
  MeasureType & value,
  DerivativeType & derivative ) const
{
  /** Create and initialize some variables. */
  this->m_NumberOfPixelsCounted = 0;
  RealType measure = NumericTraits< RealType >::Zero;
  derivative = DerivativeType( this->GetNumberOfParameters() );
  derivative.Fill( NumericTraits< DerivativeValueType >::Zero );

  SpatialHessianType spatialHessian;
  JacobianOfSpatialHessianType jacobianOfSpatialHessian;
  NonZeroJacobianIndicesType nonZeroJacobianIndices;
  unsigned long numberOfNonZeroJacobianIndices = this->m_AdvancedTransform
     ->GetNumberOfNonZeroJacobianIndices();
  jacobianOfSpatialHessian.resize( numberOfNonZeroJacobianIndices );
  nonZeroJacobianIndices.resize( numberOfNonZeroJacobianIndices );

  /** Check if the SpatialHessian is nonzero. */
  if ( !this->m_AdvancedTransform->GetHasNonZeroSpatialHessian()
    && !this->m_AdvancedTransform->GetHasNonZeroJacobianOfSpatialHessian() )
  {
    value = static_cast<MeasureType>( measure );
    return;
  }
  // TODO: This is only required once! and not every iteration.

  /** Make sure the transform parameters are up to date. */
  this->SetTransformParameters( parameters );

  /** Check if this transform is a B-spline transform. */
  typename BSplineTransformType::Pointer dummy = 0;
  bool transformIsBSpline = this->CheckForBSplineTransform( dummy );

  /** Update the imageSampler and get a handle to the sample container. */
  this->GetImageSampler()->Update();
  ImageSampleContainerPointer sampleContainer = this->GetImageSampler()->GetOutput();

  /** Create iterator over the sample container. */
  typename ImageSampleContainerType::ConstIterator fiter;
  typename ImageSampleContainerType::ConstIterator fbegin = sampleContainer->Begin();
  typename ImageSampleContainerType::ConstIterator fend = sampleContainer->End();

  /** Loop over the fixed image to calculate the penalty term and its derivative. */
  for ( fiter = fbegin; fiter != fend; ++fiter )
  {
    /** Read fixed coordinates and initialize some variables. */
    const FixedImagePointType & fixedPoint = (*fiter).Value().m_ImageCoordinates;
    MovingImagePointType mappedPoint;

    /** Although the mapped point is not needed to compute the penalty term,
     * we compute in order to check if it maps inside the support region of
     * the B-spline and if it maps inside the moving image mask.
     */

    /** Transform point and check if it is inside the B-spline support region. */
    bool sampleOk = this->TransformPoint( fixedPoint, mappedPoint );

    /** Check if point is inside mask. */
    if ( sampleOk )
    {
      sampleOk = this->IsInsideMovingMask( mappedPoint );
    }

    if ( sampleOk )
    {
      this->m_NumberOfPixelsCounted++;

      /** Get the spatial Hessian of the transformation at the current point.
       * This is needed to compute the bending energy.
       */
//       this->m_AdvancedTransform->GetSpatialHessian( fixedPoint,
//         spatialHessian );
//       this->m_AdvancedTransform->GetJacobianOfSpatialHessian( fixedPoint,
//         jacobianOfSpatialHessian, nonZeroJacobianIndices );
       this->m_AdvancedTransform->GetJacobianOfSpatialHessian( fixedPoint,
         spatialHessian, jacobianOfSpatialHessian, nonZeroJacobianIndices );

      /** Prepare some stuff for the computation of the metric (derivative). */
      FixedArray< InternalMatrixType, FixedImageDimension > A;
      for ( unsigned int k = 0; k < FixedImageDimension; ++k )
      {
        A[ k ] = spatialHessian[ k ].GetVnlMatrix();
      }

      /** Compute the contribution to the metric value of this point. */
      for ( unsigned int k = 0; k < FixedImageDimension; ++k )
      {
        measure += vnl_math_sqr( A[ k ].frobenius_norm() );
      }

      /** Make a distinction between a B-spline transform and other transforms. */
      if ( !transformIsBSpline )
      {
        /** Compute the contribution to the metric derivative of this point. */
        for ( unsigned int mu = 0; mu < nonZeroJacobianIndices.size(); ++mu )
        {
          for ( unsigned int k = 0; k < FixedImageDimension; ++k )
          {
            /** This computes:
             * \sum_i \sum_j A_ij B_ij = element_product(A,B).mean()*B.size()
             */
            const InternalMatrixType & B
              = jacobianOfSpatialHessian[ mu ][ k ].GetVnlMatrix();

            RealType matrixProduct = 0.0;
            typename InternalMatrixType::const_iterator itA = A[ k ].begin();
            typename InternalMatrixType::const_iterator itB = B.begin();
            typename InternalMatrixType::const_iterator itAend = A[ k ].end();
            while ( itA != itAend )
            {
              matrixProduct += (*itA) * (*itB);
              ++itA;
              ++itB;
            }

            derivative[ nonZeroJacobianIndices[ mu ] ]
              += 2.0 * matrixProduct;
          }
        }
      }
      else
      {
        /** For the B-spline transform we know that only 1/FixedImageDimension
         * part of the JacobianOfSpatialHessian is non-zero.
         */

        /** Compute the contribution to the metric derivative of this point. */
        unsigned int numParPerDim
          = nonZeroJacobianIndices.size() / FixedImageDimension;
        /*SpatialHessianType * basepointer1 = &jacobianOfSpatialHessian[ 0 ];
        unsigned long * basepointer2 = &nonZeroJacobianIndices[ 0 ];
        double * basepointer3 = &derivative[ 0 ];*/
        for ( unsigned int mu = 0; mu < numParPerDim; ++mu )
        {
          for ( unsigned int k = 0; k < FixedImageDimension; ++k )
          {
            /** This computes:
             * \sum_i \sum_j A_ij B_ij = element_product(A,B).mean()*B.size()
             */
            /*const InternalMatrixType & B
              = (*( basepointer1 + mu + numParPerDim * k ))[ k ].GetVnlMatrix();
            const RealType matrixMean = element_product( A[ k ], B ).mean();
            *( basepointer3 + (*( basepointer2 + mu + numParPerDim * k )) )
              += 2.0 * matrixMean * Bsize;*/
            const InternalMatrixType & B
              = jacobianOfSpatialHessian[ mu + numParPerDim * k ][ k ].GetVnlMatrix();

            RealType matrixElementProduct = 0.0;
            typename InternalMatrixType::const_iterator itA = A[ k ].begin();
            typename InternalMatrixType::const_iterator itB = B.begin();
            typename InternalMatrixType::const_iterator itAend = A[ k ].end();
            while ( itA != itAend )
            {
              matrixElementProduct += (*itA) * (*itB);
              ++itA;
              ++itB;
            }

            derivative[ nonZeroJacobianIndices[ mu + numParPerDim * k ] ]
              += 2.0 * matrixElementProduct;
          }
        }
      } // end if B-spline

    } // end if sampleOk

  } // end for loop over the image sample container

  /** Check if enough samples were valid. */
  this->CheckNumberOfSamples(
    sampleContainer->Size(), this->m_NumberOfPixelsCounted );

  /** Update measure value. */
  measure /= static_cast<RealType>( this->m_NumberOfPixelsCounted );
  derivative /= static_cast<RealType>( this->m_NumberOfPixelsCounted );

  /** The return value. */
  value = static_cast<MeasureType>( measure );

} // end GetValueAndDerivative()


} // end namespace itk

#endif // #ifndef __itkTransformBendingEnergyPenaltyTerm_txx


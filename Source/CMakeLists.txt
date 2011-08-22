CMAKE_MINIMUM_REQUIRED( VERSION 2.6 )

INCLUDE(${CMAKE_SOURCE_DIR}/IJMacros.txt)

#Change PROJECT_NAME to the name of your project
PROJECT(AdvancedTransforms)

#The following lines are required to use Dart
ENABLE_TESTING()
INCLUDE(Dart)

#Declare any external dependencies that your project may have here.
#examples include: ITK, VTK, JPEG, PNG, OpenGL, ZLIB, Perl, Java
#If you're not sure what name to use, look in the Modules directory of your
#cmake install and check that a file named Find(Package).cmake exists
#
# The packages can be specified with a version number, for example:
#
# ITK 2.8.1
# ITK 3.2.0
#
# If no version is specified, the most recent release of the package
# will be used.
SET(Required_Packages
ITK
)

#this foreach loads all of the packages that you specified as required.
#It shouldn't need to be modified.
FOREACH(Package ${Required_Packages})
  LOADPACKAGE(${Package})
ENDFOREACH(Package)

#Set any libraries that your project depends on.
#examples: ITKCommon, VTKRendering, etc
SET(Libraries
ITKCommon
ITKIO
)

# Advanced transforms code
SET(AdvancedTransformCode
  Transforms/itkAdvancedBSplineDeformableTransform.h
  Transforms/itkAdvancedBSplineDeformableTransform.txx
  Transforms/itkAdvancedBSplineDeformableTransformBase.h
  Transforms/itkAdvancedBSplineDeformableTransformBase.txx
  Transforms/itkAdvancedCombinationTransform.h
  Transforms/itkAdvancedCombinationTransform.hxx
  Transforms/itkAdvancedIdentityTransform.h
  Transforms/itkAdvancedMatrixOffsetTransformBase.h
  Transforms/itkAdvancedMatrixOffsetTransformBase.txx
  Transforms/itkAdvancedRigid2DTransform.h
  Transforms/itkAdvancedRigid2DTransform.txx
  Transforms/itkAdvancedRigid3DTransform.h
  Transforms/itkAdvancedRigid3DTransform.txx
  Transforms/itkAdvancedTransform.h
  Transforms/itkAdvancedTransform.txx
  Transforms/itkBSplineDerivativeKernelFunction2.h
  Transforms/itkBSplineInterpolationDerivativeWeightFunction.h
  Transforms/itkBSplineInterpolationDerivativeWeightFunction.txx
  Transforms/itkBSplineInterpolationSecondOrderDerivativeWeightFunction.h
  Transforms/itkBSplineInterpolationSecondOrderDerivativeWeightFunction.txx
  Transforms/itkBSplineInterpolationWeightFunction2.h
  Transforms/itkBSplineInterpolationWeightFunction2.txx
  Transforms/itkBSplineInterpolationWeightFunctionBase.h
  Transforms/itkBSplineInterpolationWeightFunctionBase.txx
  Transforms/itkBSplineKernelFunction2.h
  Transforms/itkBSplineSecondOrderDerivativeKernelFunction2.h
  Transforms/itkGridScheduleComputer.h
  Transforms/itkGridScheduleComputer.txx
  Transforms/itkTransformToDeterminantOfSpatialJacobianSource.h
  Transforms/itkTransformToDeterminantOfSpatialJacobianSource.txx
)

INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR}/CostFunctions )
INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR}/ImageSamplers )
INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR}/Transforms )
INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR}/BendingEnergy )


#---------------------------------------------------------------------
# Macro that simplifies the addition of tests
#
# Usage:
# ADD_ELX_TEST( <name_of_test> )
#
# We assume here that the name of the source which includes the main is:
#   "itk"name_of_test".cxx".
#

MACRO( ADD_ELX_TEST name )

  # Define some helper variables.
  SET( ELXTEST_EXECUTABLE_NAME itk${name} )
  SET( ELXTEST_SOURCE_NAME itk${name}.cxx )
  SET( ELXTEST_TEST_NAME ${name} )

  # Create the test executable.
  ADD_EXECUTABLE( ${ELXTEST_EXECUTABLE_NAME} ${ELXTEST_SOURCE_NAME} )

  # Link against other libraries.
  TARGET_LINK_LIBRARIES( ${ELXTEST_EXECUTABLE_NAME}
    ITKCommon ITKBasicFilters )

  # Add the test.
  ADD_TEST( ${ELXTEST_TEST_NAME} ${ELXTEST_EXECUTABLE_NAME} ${ARGN} )

ENDMACRO( ADD_ELX_TEST )

#---------------------------------------------------------------------

ADD_ELX_TEST( BSplineDerivativeKernelFunctionTest )
ADD_ELX_TEST( BSplineSODerivativeKernelFunctionTest )
ADD_ELX_TEST( BSplineInterpolationWeightFunctionTest )
ADD_ELX_TEST( BSplineInterpolationDerivativeWeightFunctionTest )
ADD_ELX_TEST( BSplineInterpolationSODerivativeWeightFunctionTest )
ADD_ELX_TEST( AdvancedBSplineDeformableTransformTest ${AdvancedTransforms_SOURCE_DIR}/parameters_AdvancedBSplineDeformableTransformTest.txt)


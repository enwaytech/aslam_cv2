#include <cmath>

#include <Eigen/Core>
#include <eigen-checks/gtest.h>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <opencv2/imgproc/imgproc.hpp>

#include <aslam/cameras/camera-unified-projection.h>
#include <aslam/cameras/camera-pinhole.h>
#include <aslam/cameras/distortion-radtan.h>
#include <aslam/cameras/distortion-equidistant.h>
#include <aslam/cameras/distortion-fisheye.h>
#include <aslam/cameras/distortion-null.h>
#include <aslam/cameras/distortion-radtan.h>
#include <aslam/cameras/undistorter-mapped.h>
#include <aslam/common/entrypoint.h>
#include <aslam/common/memory.h>
#include <aslam/pipeline/test/convert-maps-legacy.h>

///////////////////////////////////////////////
// Types to test
///////////////////////////////////////////////
template<typename Camera, typename Distortion>
struct CameraDistortion {
  typedef Camera CameraType;
  typedef Distortion DistortionType;
};

using testing::Types;
typedef Types<CameraDistortion<aslam::PinholeCamera, aslam::FisheyeDistortion>,
    CameraDistortion<aslam::UnifiedProjectionCamera, aslam::FisheyeDistortion>,
    CameraDistortion<aslam::PinholeCamera, aslam::EquidistantDistortion>,
    CameraDistortion<aslam::UnifiedProjectionCamera, aslam::EquidistantDistortion>,
    CameraDistortion<aslam::PinholeCamera, aslam::RadTanDistortion>,
    CameraDistortion<aslam::UnifiedProjectionCamera, aslam::RadTanDistortion>,
    CameraDistortion<aslam::PinholeCamera, aslam::NullDistortion>,
    CameraDistortion<aslam::UnifiedProjectionCamera, aslam::NullDistortion>> Implementations;

typedef Types<CameraDistortion<aslam::UnifiedProjectionCamera, aslam::FisheyeDistortion>,
    CameraDistortion<aslam::UnifiedProjectionCamera, aslam::EquidistantDistortion>,
    CameraDistortion<aslam::UnifiedProjectionCamera, aslam::RadTanDistortion>,
    CameraDistortion<aslam::UnifiedProjectionCamera, aslam::NullDistortion>>
    ImplementationsNoPinhole;

///////////////////////////////////////////////
// Test fixture
///////////////////////////////////////////////
template <class CameraDistortion>
class TestUndistorters : public testing::Test {
 public:
  typedef typename CameraDistortion::CameraType CameraType;
  typedef typename CameraDistortion::DistortionType DistortionType;
  protected:
  TestUndistorters() : camera_(CameraType::template createTestCamera<DistortionType>() ) {};
    virtual ~TestUndistorters() {};
    typename CameraType::Ptr camera_;
};

template <class CameraDistortion>
class TestUndistortersNoPinhole : public TestUndistorters<CameraDistortion> { };

TYPED_TEST_CASE(TestUndistorters, Implementations);
TYPED_TEST_CASE(TestUndistortersNoPinhole, ImplementationsNoPinhole);

///////////////////////////////////////////////
// Generic test cases (run for all models)
///////////////////////////////////////////////
TYPED_TEST(TestUndistorters, TestMappedUndistorter) {
  std::unique_ptr<aslam::MappedUndistorter> undistorter =
      aslam::createMappedUndistorter(*(this->camera_), 1.0, 1.0,
                                     aslam::InterpolationMethod::Linear);
  ASSERT_EQ(undistorter->getOutputCamera().getType(), undistorter->getInputCamera().getType());
  ASSERT_EQ(undistorter->getOutputCamera().getDistortion().getType(),
            aslam::Distortion::Type::kNoDistortion);

  // Test the undistortion on some points.
  const double ru = undistorter->getOutputCamera().imageWidth();
  const double rv = undistorter->getOutputCamera().imageHeight();

  for (double u = 0; u < ru; ++u) {
    for (double v = 0; v < rv; ++v) {
      // Create random keypoint (round the coordinates to avoid interpolation on the maps)
      Eigen::Vector2d keypoint_undistorted(u, v);

      // Distort using internal camera functions.
      Eigen::Vector2d keypoint_distorted;
      Eigen::Vector3d point_3d;

      CHECK(undistorter->getOutputCamera().backProject3(keypoint_undistorted, &point_3d));
      point_3d /= point_3d[2];
      this->camera_->project3(point_3d, &keypoint_distorted);

      Eigen::Vector2d keypoint_distorted_maps;
      undistorter->processPoint(keypoint_undistorted, &keypoint_distorted_maps);
      EXPECT_TRUE(EIGEN_MATRIX_NEAR(keypoint_distorted, keypoint_distorted_maps, 5e-2));
    }
  }
}

////////////////////////////////////
// Camera model specific test cases
////////////////////////////////////
TEST(TestUndistortersNoPinhole, TestMappedUndistorterUpcToPinhole) {
  aslam::UnifiedProjectionCamera::Ptr camera = aslam::UnifiedProjectionCamera::createTestCamera<
      aslam::RadTanDistortion>();

  std::unique_ptr<aslam::MappedUndistorter> undistorter =
      aslam::createMappedUndistorterToPinhole(*camera, 1.0, 1.0,
                                              aslam::InterpolationMethod::Linear);
  ASSERT_EQ(undistorter->getOutputCamera().getType(), aslam::Camera::Type::kPinhole);
  ASSERT_EQ(undistorter->getOutputCamera().getDistortion().getType(),
            aslam::Distortion::Type::kNoDistortion);

  // Test the undistortion on some random points.
  const double ru = undistorter->getOutputCamera().imageWidth();
  const double rv = undistorter->getOutputCamera().imageHeight();

  for (double u = 0; u < ru; ++u) {
    for (double v = 0; v < rv; ++v) {
      // Create random keypoint (round the coordinates to avoid interpolation on the maps)
      Eigen::Vector2d keypoint_undistorted(u, v);
      keypoint_undistorted[0] = std::floor(keypoint_undistorted[0]);
      keypoint_undistorted[1] = std::floor(keypoint_undistorted[1]);

      // Distort using internal camera functions.
      Eigen::Vector2d keypoint_distorted;
      Eigen::Vector3d point_3d;
      undistorter->getOutputCamera().backProject3(keypoint_undistorted, &point_3d);
      point_3d /= point_3d[2];
      camera->project3(point_3d, &keypoint_distorted);

      Eigen::Vector2d keypoint_distorted_maps;
      undistorter->processPoint(keypoint_undistorted, &keypoint_distorted_maps);
      EXPECT_TRUE(EIGEN_MATRIX_NEAR(keypoint_distorted, keypoint_distorted_maps, 5e-2));
    }
  }
}

ASLAM_UNITTEST_ENTRYPOINT
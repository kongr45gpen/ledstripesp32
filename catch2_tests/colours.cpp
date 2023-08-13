#include "catch2/catch.hpp"
#include "color/color.hpp"
#include <complex>
#include <Eigen/Dense>
#include <iostream>

using namespace Catch::literals;



void mix(color::xyz<double> color) {
    // Perform simplex to minimise f=dT*Q
    // where dT is the temperature difference and Q is the heat capacity
    // of the material.
    //

    Eigen::Vector4d Q;
    Q << 1.0, 1.0, 1.0, 0.0;

    color::rgb<uint8_t> red({255, 0, 0});
    color::rgb<uint8_t> green({0, 255, 0});
    color::rgb<uint8_t> blue({0, 0, 255});
    color::rgb<uint8_t> white({255, 255, 255});
    color::xyz<double> red_xyz;
    color::xyz<double> green_xyz;
    color::xyz<double> blue_xyz;
    color::xyz<double> white_xyz;
    red_xyz = red;
    green_xyz = green;
    blue_xyz = blue;
    white_xyz = white;
}

color::rgb<uint8_t> red({255, 0, 0});
color::rgb<uint8_t> green({0, 255, 0});
color::rgb<uint8_t> blue({0, 0, 255});
color::rgb<uint8_t> white({255, 255, 255});

TEST_CASE( "RGB to XYZ conversion" ) {
    color::xyz<double> red_xyz;
    color::xyz<double> green_xyz;
    color::xyz<double> blue_xyz;
    color::xyz<double> white_xyz;

    red_xyz = red;
    green_xyz = green;
    blue_xyz = blue;
    white_xyz = white;

    CHECK(red_xyz[0] == 41.2456_a);
    CHECK(red_xyz[1] == 21.2673_a);
    CHECK(red_xyz[2] == 1.93339_a);

    CHECK(green_xyz[0] == 35.7576_a);
    CHECK(green_xyz[1] == 71.5152_a);
    CHECK(green_xyz[2] == 11.9192_a);

    CHECK(blue_xyz[0] == 18.0437_a);
    CHECK(blue_xyz[1] == 7.2175_a);
    CHECK(blue_xyz[2] == 95.0304_a);

    CHECK(white_xyz[0] == 95.047_a);
    CHECK(white_xyz[1] == 100.0_a);
    CHECK(white_xyz[2] == 108.883_a);
}

TEST_CASE( "RGBW color mixing" ) {
    color::xyy<double> red_xyy(red);
    color::xyy<double> green_xyy(green);
    color::xyy<double> blue_xyy(blue);
    color::xyy<double> white_xyy(white);

    CHECK(red_xyy[0] == 0.6400_a);
    CHECK(red_xyy[1] == 0.3300_a);
    CHECK(red_xyy[2] == 21.2673_a);

    CHECK(green_xyy[0] == 0.3000_a);
    CHECK(green_xyy[1] == 0.6000_a);
    CHECK(green_xyy[2] == 71.5152_a);

    CHECK(blue_xyy[0] == 0.1500_a);
    CHECK(blue_xyy[1] == 0.0600_a);
    CHECK(blue_xyy[2] == 7.2175_a);

    CHECK(white_xyy[0] == 0.312727_a);
    CHECK(white_xyy[1] == 0.329023_a);
    CHECK(white_xyy[2] == 100_a);

    color::rgb<double> target ({0, 0, 255}); // a nice orange
    color::xyy<double> target_xyy(target);

    // Moore-Penrose inverse, based on Kim et. al. (2009)
    Eigen::Matrix<double, 3, 4> A;
    A(0,0) = (red_xyy[0] - target_xyy[0]) / red_xyy[1];
    A(0,1) = (green_xyy[0] - target_xyy[0]) / green_xyy[1];
    A(0,2) = (blue_xyy[0] - target_xyy[0]) / blue_xyy[1];
    A(0,3) = (white_xyy[0] - target_xyy[0]) / white_xyy[1];
    A(1,0) = (red_xyy[1] - target_xyy[1]) / red_xyy[1];
    A(1,1) = (green_xyy[1] - target_xyy[1]) / green_xyy[1];
    A(1,2) = (blue_xyy[1] - target_xyy[1]) / blue_xyy[1];
    A(1,3) = (white_xyy[1] - target_xyy[1]) / white_xyy[1];
    A(2,0) = 1.0;
    A(2,1) = 1.0;
    A(2,2) = 1.0;
    A(2,3) = 1.0;

    // Eigen::Matrix<double, 4, 3> A_inv;
    // A_inv = A.completeOrthogonalDecomposition().pseudoInverse();

    auto A_inv = A.completeOrthogonalDecomposition().pseudoInverse();

    Eigen::Vector3d B = {0, 0, 1.0};

    auto I = A_inv * B;
    std::cout << "\nMatrix A:\n" << A << std::endl;
    std::cout << "\nMatrix A_inv:\n" << A_inv << std::endl;
    std::cout << "\nMatrix I:\n" << I << std::endl;

    color::xyy<double> combined = I[0] * red + I[1] * green + I[2] * blue + I[3] * white;
    color::rgb<double> combined_rgb(combined);

    CHECK(combined_rgb[0] == 0_a);
    CHECK(combined_rgb[1] == 0_a);
    CHECK(combined_rgb[2] == 255_a);
}
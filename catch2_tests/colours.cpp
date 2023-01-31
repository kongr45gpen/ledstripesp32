#include "catch2/catch.hpp"
#include "color/color.hpp"
#include <complex>
#include <Eigen/Dense>

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

TEST_CASE( "RGB to XYZ conversion" ) {
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
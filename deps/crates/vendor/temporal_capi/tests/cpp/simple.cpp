#include <temporal_rs/Calendar.hpp>
#include <temporal_rs/PlainDate.hpp>

#include <iostream>

using namespace temporal_rs;

int main() {
    auto date = PlainDate::try_new_with_overflow(2025, 1, 33, AnyCalendarKind::Gregorian, ArithmeticOverflow::Constrain).ok().value();

    auto formatted = date->to_ixdtf_string(DisplayCalendar::Always);

    std::cout<<formatted<<std::endl;

}
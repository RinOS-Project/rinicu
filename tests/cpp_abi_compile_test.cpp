/* SPDX-License-Identifier: MIT */
#include <cassert>
#include <type_traits>

#include <rinicu/rinicu.hpp>

int main()
{
    static_assert(!std::is_copy_constructible<RinICU::Client>::value);
    static_assert(!std::is_move_constructible<RinICU::Client>::value);
    RinICU::Result<int> failed = { RIN_ICU_STATUS_IO_ERROR, 0 };
    RinICU::Client* client = nullptr;
    (void)client;
    assert(!failed.ok());
    return 0;
}

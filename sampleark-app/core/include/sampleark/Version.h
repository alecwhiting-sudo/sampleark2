#pragma once

namespace sampleark
{
// Single source of truth for the core version. The headless core is intentionally
// free of any UI/framework dependency so it can be unit-tested on its own.
const char* coreVersionString() noexcept;
}

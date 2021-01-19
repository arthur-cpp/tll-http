#include <string_view>
#include "tll/scheme/types.h"

namespace lws_scheme {
static constexpr std::string_view scheme = R"(yamls://
- name: connect
  id: 1
  fields:
    - { name: path, type: string }
- name: disconnect
  id: 2
  fields:
    - { name: code, type: uint16 }
)";

struct connect {
	static constexpr int id = 1;
	tll::scheme::offset_ptr_t<char> path;
};

struct disconnect {
	static constexpr int id = 2;
	uint16_t code;
};
}

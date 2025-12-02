#pragma once

#define WIN32_LEAN_AND_MEAN
#pragma comment(lib, "ws2_32.lib")

#include <errno.h>
#include <optional>
#include <stdint.h>
#include <source_location>
#include <iomanip>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <thread>
#include <span>
#include <string>

using i32 = int32_t;
using u16 = uint16_t;
using u32 = uint32_t;
using usize = size_t;
using String = std::string;
using StringView = std::string_view;
using Bytes = std::span<const std::byte>;
using WritableBytes = std::span<std::byte>;

/**
 * @brief Prints out all arguments in order
 */
template<typename... Args>
auto print(Args&&... args) -> void {
  (std::cout << ... << std::forward<Args>(args));
}

/**
 * @brief Prints out all arguments in order to std::cerr
 */
template<typename... Args>
auto eprint(Args&&... args) -> void {
  (std::cout << ... << std::forward<Args>(args));
}

/**
 * @brief Prints out all arguments in order to standard error
 */
template<typename... Args>
auto println(Args&&... args) {
  print(std::forward<Args>(args)...);
  std::cout << std::endl;
}

#if !SUBMISSION
/**
 * @brief Prints out all arguments in order with a newline at the end, does
 * nothing during submission
 */
template<typename... Args>
auto dbgprintln(Args&&... args) {
  print(std::forward<Args>(args)...);
  std::cout << std::endl;
}
#else
/**
 * @brief Prints out all arguments in order with a newline at the end, does
 * nothing during submission
 */
template<typename... Args>
auto dbgprintln([[maybe_unused]] Args&&... args) {}
#endif

/**
 * @brief Prints arguments in order to standard error and then a newline
 */
template<typename... Args>
auto eprintln(Args&&... args) {
  eprint(std::forward<Args>(args)...);
  std::cerr << std::endl;
}


/**
 * @class SocketError
 * @brief Error for any WinSock API call
 *
 */
class SocketError final : public std::runtime_error {
public:

  explicit SocketError(
    const String& message,
    const errno_t error = WSAGetLastError(),
    const std::source_location& location = std::source_location::current()
  );

  errno_t code;
};

/**
 * RAII wrapper for WinSockAPI because I dont want to use goto
 */
class Wsa final {
public:

  Wsa();

  Wsa(const Wsa&) = delete;

  Wsa(Wsa&&) noexcept = delete;

  Wsa& operator=(const Wsa&) = delete;

  Wsa& operator=(Wsa&& other) noexcept = delete;

  ~Wsa();

  WSAData data;
};

struct SocketClient;

/**
 * Wrapper for a socket
 */
class Socket final {
public:

  explicit Socket(const SOCKET handle);

  /**
   * Create socket
   */
  static auto create(
    const i32 af = AF_INET,
    const i32 stream = SOCK_STREAM,
    const i32 protocol = IPPROTO_TCP
  ) -> Socket;

  /**
   * @brief Attempt to connect this socket to the given INET address
   */
  auto connect(const sockaddr_in address) -> void;

  /**
   * @brief Attempt to connect this socket to the given address
   */
  auto connect(const sockaddr* address, const usize address_size) -> void;

  /**
   * @brief Attempt to connect this socket to the given IPv4 adress & port (this
   * will parse the parameters)
   */
  auto connect(const char* ipv4, const u16 port) -> void;

  /**
   * @brief Bind this socket to an address
   */
  auto bind(const sockaddr_in& address) -> void;

  /**
   * @brief Sets this socket to be nonblocking or blocking
   */
  auto set_nonblocking(const bool is_nonblocking = true) -> void;

  /**
   * @brief Open this socket for incoming connections
   *
   * @param backlog
   */
  auto listen(const i32 backlog = SOMAXCONN) const -> void;

  /**
   * Accept a new client for a listening socket
   */
  auto accept() const -> std::optional<SocketClient>;

  /**
   * Send data
   */
  auto send(const StringView data, const i32 flags = 0) const -> void;

  /**
   * Send bytes
   */
  auto send(const Bytes data, const i32 flags = 0) const -> void;

  /**
   * Received bytes to a buffer, returns a span of the received data.
   */
  [[nodiscard]] auto receive(
    const std::span<char> buffer,
    const i32 flags = 0
  ) const -> Bytes;

  /**
   * Received bytes to a buffer, returns a span of the received data.
   */
  [[nodiscard]] auto receive(
    const WritableBytes buffer,
    const i32 flags = 0
  ) const -> Bytes;

  /**
   * Shutdown sending capabilities for this socket
   */
  auto stop_sending() const -> void;

  /**
   * Shutdown receiving capabilities for this socket
   */
  auto stop_receiving() const -> void;

  /**
   * Shutdown sending and receiving capabilities for this socket
   */
  auto shutdown() const -> void;

  /**
   * Converts the IPv4 string and port into a usable socket address
   */
  static auto get_address(const char* ipv4, const u16 port) -> sockaddr_in;

  /**
   * Creates a socket connected for the given hostname assuming this is a HTTP
   * interaction
   */
  static auto create_for_http(const char* hostname) -> Socket;

  auto init_for_icmp(const char* hostname) -> sockaddr_in;

  /**
   * Gets address for a given port (localhost)
   */
  static auto get_address_any_ipv4(const u16 port) -> sockaddr_in;

  Socket(const Socket& other) = delete;

  Socket& operator=(const Socket& other) = delete;

  Socket(Socket&& other) noexcept;

  Socket& operator=(Socket&& other) noexcept;

  ~Socket();

  SOCKET handle{};
};

/**
 * Client that was a result of accepting from a listening socket
 */
struct SocketClient final {
  Socket socket{INVALID_SOCKET};
  sockaddr_storage address{};
  usize address_size{};
};

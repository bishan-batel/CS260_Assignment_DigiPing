#include "WinSock.h"

SocketError::SocketError(const String& message,
                         const errno_t error,
                         const std::source_location& location):
  runtime_error{std::format(
    "[{}:{}] {}: WSAError = {}",
    location.function_name(),
    location.line(),
    message,
    error
  )},
  code{error} {}

Wsa::Wsa() {
  if (WSAStartup(MAKEWORD(2, 2), &data) != NO_ERROR) {
    eprintln("Failed WSAStartup");
  }
}

Wsa::~Wsa() { WSACleanup(); }

Socket::Socket(const SOCKET handle):
  handle{handle} {}

auto Socket::create(const i32 af,
                    const i32 stream,
                    const i32 protocol) -> Socket {
  return Socket{socket(af, stream, protocol)};
}

auto Socket::connect(const sockaddr_in address) -> void {
  connect(reinterpret_cast<const sockaddr*>(&address), sizeof(address));
}

auto Socket::connect(const sockaddr* address,
                     const usize address_size) -> void {
  const errno_t error{
    ::connect(handle, address, static_cast<int>(address_size)),
  };

  if (error == SOCKET_ERROR) {
    throw SocketError{"Failed to connect socket"};
  }
}

auto Socket::connect(const char* ipv4, const u16 port) -> void {
  connect(get_address(ipv4, port));
}

auto Socket::bind(const sockaddr_in& address) -> void {
  const errno_t error{
    ::bind(
      handle,
      reinterpret_cast<const sockaddr*>(&address),
      sizeof(sockaddr_in)
    ),
  };

  if (error == SOCKET_ERROR) {
    throw SocketError{"Failed to bind to socket"};
  }
}

auto Socket::set_nonblocking(const bool is_nonblocking) -> void {
  u_long mode{
    static_cast<u_long>(is_nonblocking ? 0 : 1),
  };

  if (ioctlsocket(handle, FIONBIO, &mode) != 0) {
    throw SocketError{
      String{"Failed to change nonblocking state to "}
      + (is_nonblocking ? "true" : "false")
    };
  }
}

auto Socket::listen(const i32 backlog) const -> void {
  if (::listen(handle, backlog) == SOCKET_ERROR) {
    throw SocketError{"Failed to set socket to listen"};
  }
}

auto Socket::send(const StringView data, const i32 flags) const -> void {
  send(std::as_bytes(std::span{data.data(), data.length()}), flags);
}

auto Socket::send(const Bytes data, const i32 flags) const -> void {
  const errno_t error{::send(
    handle,
    reinterpret_cast<const char*>(data.data()),
    static_cast<i32>(data.size_bytes()),
    flags
  )};
  if (error == SOCKET_ERROR) {
    throw SocketError{"Failed to send from socket"};
  }
}

auto Socket::receive(const std::span<char> buffer,
                     const i32 flags) const -> Bytes {
  return receive(std::as_writable_bytes(buffer), flags);
}

auto Socket::receive(const WritableBytes buffer,
                     const i32 flags) const -> Bytes {
  const int bytes_received{
    ::recv(
      handle,
      reinterpret_cast<char*>(buffer.data()),
      static_cast<i32>(buffer.size()),
      flags
    ),
  };

  if (bytes_received > 0) {
    return Bytes{buffer.data(), static_cast<usize>(bytes_received)};
  }

  if (bytes_received == 0) {
    return Bytes{};
  }

  throw SocketError{"Failed to receive from socket"};
}

auto Socket::stop_sending() const -> void { ::shutdown(handle, SD_SEND); }

auto Socket::stop_receiving() const -> void { ::shutdown(handle, SD_RECEIVE); }

auto Socket::shutdown() const -> void { ::shutdown(handle, SD_BOTH); }

auto Socket::get_address(const char* ipv4, const u16 port) -> sockaddr_in {
  sockaddr_in address{};
  memset(&address.sin_zero, 0, 8);

  // IPv4 Address
  address.sin_family = AF_INET;

  // Host port
  if (port != 0) address.sin_port = htons(port);

  // Convert ipv4 addres text representation to binary form
  if (inet_pton(AF_INET, ipv4, &address.sin_addr) != 1) {
    throw SocketError{"Failed to convert address"};
  }

  return address;
}

auto Socket::create_for_http(const char* hostname) -> Socket {
  try {
    Socket socket{create()};
    socket.connect(get_address(hostname, 80));
    return socket;
  } catch (const SocketError& ex) {
    std::ignore = ex;
  }

  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* result;

  if (getaddrinfo(hostname, "80", &hints, &result) != 0) {
    throw SocketError{"Failed to get HTTP address: to connect"};
  }

  for (addrinfo* address = result; address; address = address->ai_next) {
    try {
      Socket socket{
        create(address->ai_family, address->ai_socktype, address->ai_protocol)
      };

      socket.connect(address->ai_addr, address->ai_addrlen);
      freeaddrinfo(result);
      return socket;

    } catch (const SocketError& ex) {
      std::ignore = ex;
    } catch (const std::exception&) {
      freeaddrinfo(result);
      throw;
    }
  }

  freeaddrinfo(result);
  throw SocketError{"Failed to find suitable HTTP address to connect."};
}

auto Socket::init_for_icmp(const char* hostname) -> sockaddr_in {
  try {
    sockaddr_in addr{get_address(hostname, 0)};
    connect(addr);
    return addr;
  } catch (const SocketError& ex) {
    std::ignore = ex;
  }

  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* result;

  if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
    throw SocketError{"Failed to get address: to connect"};
  }

  for (addrinfo* address = result; address; address = address->ai_next) {
    try {
      // *this = create(
      //   address->ai_family,
      //   address->ai_socktype,
      //   address->ai_protocol
      // );

      connect(address->ai_addr, address->ai_addrlen);
      freeaddrinfo(result);
    } catch (const SocketError& ex) {
      std::ignore = ex;
    } catch (const std::exception&) {
      freeaddrinfo(result);
      throw;
    }
  }

  freeaddrinfo(result);
  throw SocketError{"Failed to find suitable HTTP address to connect."};
}

auto Socket::get_address_any_ipv4(const u16 port) -> sockaddr_in {
  sockaddr_in address{};
  memset(&address.sin_zero, 0, 8);

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  return address;
}

Socket::Socket(Socket&& other) noexcept:
  handle(std::exchange(other.handle, INVALID_SOCKET)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  handle = std::exchange(other.handle, INVALID_SOCKET);

  return *this;
}

Socket::~Socket() {
  if (handle == INVALID_SOCKET) {
    return;
  }

  if (closesocket(handle) == SOCKET_ERROR) {
    eprintln("Failed to close socket " + std::to_string(handle));
  }
}

auto Socket::accept() const -> std::optional<SocketClient> {
  SocketClient client{Socket{INVALID_SOCKET}};

  i32 size{sizeof(client.address)};

  const SOCKET client_handle{
    ::accept(handle, reinterpret_cast<sockaddr*>(&client.address), &size)
  };

  if (client_handle != INVALID_SOCKET) {
    client.socket = Socket{client_handle};
    client.address_size = static_cast<usize>(size);

    return client;
  }

  if (WSAGetLastError() != WSAEWOULDBLOCK) {
    throw SocketError{"Error receiving"};
  }

  return std::nullopt;
}

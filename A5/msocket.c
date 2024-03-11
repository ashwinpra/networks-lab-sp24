int m_socket(int domain, int type, int protocol) {
  int fd = socket(domain, type, protocol);
  if (fd < 0) {
    perror("socket");
    exit(1);
  }
  return fd;
}
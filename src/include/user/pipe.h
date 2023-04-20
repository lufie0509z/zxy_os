#ifndef __USER_PIPE_H
#define __USER_PIPE_H

#define PIPE_FLAG 0xFFFF

bool is_pipe(uint32_t local_fd);
int32_t sys_pipe(int32_t pipefd[2]);

uint32_t pipe_read(int32_t local_fd, void* buf, uint32_t cnt);
uint32_t pipe_write(int32_t local_fd, const void* buf, uint32_t cnt);

void sys_fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd);
#endif
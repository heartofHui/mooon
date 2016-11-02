/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: eyjian@qq.com or eyjian@gmail.com or eyjian@live.com
 */
#include <mooon/sys/safe_logger.h>
#include <mooon/sys/thread_engine.h>
#include <mooon/sys/utils.h>
#include <mooon/utils/args_parser.h>
#include <mooon/utils/string_utils.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

INTEGER_ARG_DEFINE(int, threads, 10, 1, 100, "number of threads");
INTEGER_ARG_DEFINE(int, processes, 10, 1, 100, "number of processes");
INTEGER_ARG_DEFINE(int, lines, 10000, 1, 100000000, "number of lines");
INTEGER_ARG_DEFINE(uint32_t, size, 1024*1024*800, 1024, 1024*1024*2000, "size of a single log file");
INTEGER_ARG_DEFINE(uint16_t, backup, 100, 1, 1000, "backup number of log file");
MOOON_NAMESPACE_USE

static atomic_t sg_counter = 0;
static void foo()
{
    for (int i=0,j=0; i<argument::lines->value(); ++i)
    {
        // 借助j和counter可观察在哪儿丢了日志，如果存在这个情况

        ++j;
        unsigned int counter = static_cast<unsigned int>(atomic_inc_return(&sg_counter));
        /* 1 */ MYLOG_DEBUG("[%u,%d]MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM\n", counter, j);

        ++j;
        counter = static_cast<unsigned int>(atomic_inc_return(&sg_counter));
        /* 2 */ MYLOG_DETAIL("[%u,%d]KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK\n", counter, j);

        ++j;
        counter = static_cast<unsigned int>(atomic_inc_return(&sg_counter));
        /* 3 */ MYLOG_INFO("[%u,%d]BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB\n", counter, j);

        ++j;
        counter = static_cast<unsigned int>(atomic_inc_return(&sg_counter));
        /* 4 */ MYLOG_ERROR("[%u,%d]TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT\n", counter, j);

        ++j;
        counter = static_cast<unsigned int>(atomic_inc_return(&sg_counter));
        /* 5 */ MYLOG_WARN("[%u,%d]PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP\n", counter, j);

        ++j;
        counter = static_cast<unsigned int>(atomic_inc_return(&sg_counter));
        /* 6 */ MYLOG_TRACE("[%u,%d]AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n", counter, j);

        ++j;
        counter = static_cast<unsigned int>(atomic_inc_return(&sg_counter));
        /* 7 */ MYLOG_STATE("[%u,%d]ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ\n", counter, j);
        //sys::CUtils::millisleep(10);
    }

    MYLOG_RELEASE();
}

// 1073741824 = 1024*1024*1024
// 838860800 = 1024*1024*800
// 压测非滚动时：./test_safe_logger --lines=10000 --size=1073741824 --processes=6 --threads=12
// 压测滚动1：./test_safe_logger --lines=10000 --size=1024000 --processes=1 --threads=1
// 压测滚动2：./test_safe_logger --lines=100 --size=1024000 --processes=10 --threads=1
// 压测滚动3：./test_safe_logger --lines=100 --size=1024000 --processes=1 --threads=10
// 压测滚动4：./test_safe_logger --lines=100 --size=1024000 --processes=2 --threads=10
int main(int argc, char* argv[])
{
    std::string errmsg;
    if (!utils::parse_arguments(argc, argv, &errmsg))
    {
        fprintf(stderr, "%s\n", errmsg.c_str());
        exit(1);
    }

    try
    {
        pid_t pid;
        ::mooon::sys::g_logger = sys::create_safe_logger(".", "test");
        sys::g_logger->set_single_filesize(argument::size->value());
        sys::g_logger->set_backup_number(argument::backup->value());
        sys::g_logger->set_log_level(sys::LOG_LEVEL_DETAIL);
        sys::g_logger->enable_trace_log(true);

        for (int i=0; i<argument::processes->value(); ++i)
        {
            pid = fork();
            if (-1 == pid)
            {
                fprintf(stderr, "fork error: %m\n");
                exit(1);
            }
            else if (0 == pid)
            {
                // 子进程
                sys::CThreadEngine** threads = new sys::CThreadEngine*[argument::threads->value()];

                for (int i=0; i<argument::threads->value(); ++i)
                {
                    threads[i] = new sys::CThreadEngine(sys::bind(&foo));
                }
                for (int i=0; i<argument::threads->value(); ++i)
                {
                    threads[i]->join();
                    delete threads[i];
                }
                delete []threads;

                MYLOG_RELEASE();
                exit(0);
            }
        }

        MYLOG_INFO("hello\n");
        MYLOG_ERROR("%s\n", "world");
        MYLOG_RELEASE();
        delete ::mooon::sys::g_logger;
        ::mooon::sys::g_logger = NULL;

        // 等待所有子进程结束
        while (true)
        {
            int status = -1;
            pid = wait(&status);
            if (pid > 0)
            {
                printf("process[%u] exit with %d\n", pid, status);
            }
            else
            {
                if (ECHILD == errno)
                {
                    break;
                }
                else
                {
                    printf("unknown error when waiting child\n");
                    break;
                }
            }
        }

        int thread_lines = 7 * mooon::argument::lines->value(); // foo函数输出了7行日志
        int all_threads_lines = thread_lines * argument::threads->value();
        int all_processes_lines = all_threads_lines * mooon::argument::processes->value();
        int total_lines = 2 + all_processes_lines;
        fprintf(stdout, "thread lines: %d\n", thread_lines);
        fprintf(stdout, "all threads lines: %d\n", all_threads_lines);
        fprintf(stdout, "all processes lines: %d\n", all_processes_lines);
        fprintf(stdout, "total lines: %d\n", total_lines);
        fprintf(stdout, "press ENTER to exit\n");
        getchar();
    }
    catch (sys::CSyscallException& syscall_ex)
    {
        fprintf(stderr, "%s\n", syscall_ex.str().c_str());
        exit(1);
    }

    return 0;
}

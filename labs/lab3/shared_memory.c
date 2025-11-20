#include "./shared_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // close(), fork()
#include <fcntl.h>      // флаги по типу O_CREAT, O_RDWR
#include <sys/stat.h>   // для работы с правами доступа
#include <sys/wait.h>   // для ожидания завершения активных процессов


shared_memory *create_shared_memory(const char* name) {
    if (shm_unlink(name) == -1 && errno != ENOENT) {
        perror("shm_unlink in create");
    }

    /*
    shm_open() - функция для создания/открытия объекта разделяемой памяти (shared memory)
    Принимает:
        - name - имя объекта
        - O_CREAT | O_RDWR - флаги: в данном случае - (создать если не существует) и открыть для чтения и записи 
        - 0666 - права доступа (чтение и запись для всех)
    Возвращает файловый дескриптор в случае успеха, иначе -1 = ошибка
    */
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);    
    if (fd == -1) {
        /*
        perror - выводит в stderr сообщение об ошибке, а затем переданное пользовательское сообщение
        */
        perror("shm_open");     
        exit(EXIT_FAILURE);
    }

    /*
    ftruncate() - устанавливает размер файла в указанное значение
    Принимает:
        - fd - файловый дескриптор, которому нужно установить новый размер
        - sizeof() - новый размер
    Возвращает 0 при успехе, -1 при ошибке

    Использование ftruncate64(): требуеься только если нужны файлы больше 2GB на 32-битных системах
    */
    if (ftruncate(fd, sizeof(shared_memory)) == -1) {   
        close(fd);
        /*
        shm_unlink() - удаляет имя объекта shared memory. После этого, когда все процессы отобразят объект, он уничтожается
        Принимает: 
            - name - имя объекта
        */
        shm_unlink(name);     
        exit(EXIT_FAILURE);
    }

    /*
    mmap() - отображение shared memory в адресное пространство процесса.
    Принимает:
        - NULL - предпочтительный адрес отображения (NULL даёт системе выбрать самой)
        - sizeof() - размер отомбражаемой части
        - PROT_READ | PROT_WRITE - прав доступа: чтение и доступ
        - MAP_SHARED - флаг, что изменения видны другим процессам
        - fd - соответствующий файловый дескриптор
        - 0 - смещение в файле на <0> байт
    Возвращает: указатель на память, где расположен, в случае успеха и MAP_FAILED в случае неудачи
    */
    shared_memory *shm = mmap(NULL, sizeof(shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);  // PROT = protection, предположительно, безопасные константы    
    if (shm == MAP_FAILED) {
        perror("mmap");
        close(fd);
        shm_unlink(name);
        exit(EXIT_FAILURE);
    }

    close(fd);      // после отображения файла в памяти в дескрипторе нет необходимости
    shm->data_ready = 0;
    shm->process_complete = 0;
    return shm;
}


shared_memory *open_shared_memory(const char* name) {
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    shared_memory *shm = mmap(NULL, sizeof(shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // аналогично функции в create функции
    if (shm == MAP_FAILED) {
        perror("mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
    return shm;
}

// unlink - флаг, нужно ли удалять сегмент из shared memory -- у родительского 1, у дочернего 0
void close_shared_memory(shared_memory *shm, const char* name, int unlink) { 
    if (shm == NULL) return;

    /*
    munmap() - отменяет отображение shared memory из адресного пространства процесса.
    Принимает:
        - shm - указатель на начало отображения
        - sizeof() - размер отображения
    Возвравщает: 0 при успехе, -1 в случае ошибки
    Примечание: полсе munmap обращение по указателю shm будет некорректным
    */
    if(munmap(shm, sizeof(shared_memory)) == -1) {
        perror("munmap");
    }

    if (unlink && shm_unlink(name) == -1) {
        perror("shm_unlink"); 
    }
}


pid_t fork_process() {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    return pid;
}


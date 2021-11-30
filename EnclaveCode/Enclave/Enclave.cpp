/*
 * Copyright (C) 2011-2021 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "Enclave.h"
#include "Enclave_t.h" /* print_string */
#include <stdarg.h>
#include <stdio.h> /* vsnprintf */
#include <string.h>
#include <unistd.h>
#include <sgx_rsrv_mem_mngr.h>
/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */
Student *current_student;
Student *first_student;
Student *last_student;
Class *current_class;
Class *first_class;
Class *last_class;
int printf(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
    return (int)strnlen(buf, BUFSIZ - 1) + 1;
}
void fill_mem()
{
    uint64_t page_size = getpagesize();
    void *ret = NULL;
    int count = 0;
    while (true)
    {
        ret = sgx_alloc_rsrv_mem(page_size);
        printf("%d\n", count++);
    }
}
//searches for a class by its id
Class *find_class(int id)
{
    current_class = first_class;
    while (current_class != NULL)
    {
        if (id == current_class->id)
        {
            return current_class;
        }
        current_class = current_class->next_class;
    }
    return NULL;
}

Student *find_student(int id)
{
    current_student = first_student;
    while (current_student != NULL)
    {
        if (id == current_student->id)
        {
            return current_student;
        }
        current_student = current_student->next_student;
    }
    return NULL;
}

void creation_error(void *address, int aligned_size)
{
    sgx_free_rsrv_mem(address, aligned_size);
}
float calc_gpa(Student *address)
{
    Personal_Class *current = address->first_class;
    int credits = 0;
    int perc_total = 0;
    while (current != NULL)
    {
        perc_total += current->percentage * current->current_class->credits;
        credits += current->current_class->credits;
        current = current->next_class;
    }
    return perc_total / credits;
}
//creates n new students, return 0 if successful, -1 if error
int new_student(int n)
{
    uint64_t page_size, aligned_size;
    page_size = getpagesize();
    aligned_size = ((n * sizeof(Student)) + page_size - 1) & ~(page_size - 1);
    printf("Aligned size %d\n", aligned_size);
    Student *address = (Student *)sgx_alloc_rsrv_mem(aligned_size);
    printf("Allocated memory\n");
    printf("%x\n", (int *)address);
    int id;
    char *str;
    for (int i = 0; i < n; i++)
    {
        ocall_print_string("Enter student ID\n");
        ocall_enter_int(&id);
        printf("id: %d \n", id);
        (address + i)->id = id;
        printf("id assigned\n");
        ocall_print_string("Enter first name\n");
        ocall_enter_string(address->first_name);
        ocall_print_string("Enter last name\n");
        ocall_enter_string(address->last_name);
        ocall_print_string("How many classes have they taken?\n");
        int m;
        ocall_enter_int(&m);
        for (int j = 0; j < m; j++)
        {
            ocall_print_string("Enter ID of class\n");
            ocall_enter_int(&id);
            Class *c = find_class(id);
            if (c == NULL)
            {
                ocall_print_string("No class found, creating new class\n");
                int ret = new_class(1);
                if (ret == 0)
                {
                    c = last_class;
                }
                else
                {
                    creation_error(address, aligned_size);
                }
            }
            uint64_t aligned_pclass_size = aligned_size = ((n * sizeof(Personal_Class)) + page_size - 1) & ~(page_size - 1);
            Personal_Class *pclass = (Personal_Class *)sgx_alloc_rsrv_mem(aligned_pclass_size);
            printf("pclass created\n");
            printf("Address: %x\n", (int *)c);
            pclass->current_class = c;
            printf("pclassed assigned\n");
            if ((address + i)->first_class == NULL)
            {
                (address + i)->first_class = pclass;
            }
            else
            {
                (address + i)->last_class->next_class = pclass;
            }
            (address + i)->last_class = pclass;
            ocall_print_string("Enter the student's percentage in this class\n");
            ocall_enter_int(&id);
            pclass->percentage = id;
            ocall_print_string("Enter any comments from the professor\n");
            ocall_enter_string(pclass->comments);
        }
        (address + i)->gpa = calc_gpa(address + i);
        if (first_student == NULL)
        {
            printf("Entered if statement\n");
            first_student = address + i;
            current_student = address + i;
            last_student = address + i;
        }
        else
        {
            int assigned = 0;
            current_student = first_student;
            while (assigned == 0)
            {
                if (current_student->next_student->id > id || current_student->next_student->id == NULL)
                {
                    (address + i)->next_student = current_student->next_student;
                    current_student->next_student = (address + i);
                    assigned = 1;
                }
            }
        }
    }
    return 0;
}

//creates n number new classes, return 0 if successful, -1 if error
int new_class(int n)
{
    printf("Entered new_class");
    uint64_t page_size, aligned_size;
    page_size = getpagesize();
    aligned_size = ((n * sizeof(Class)) + page_size - 1) & ~(page_size - 1);
    printf("Aligned size %d\n", aligned_size);
    Class *address = (Class *)sgx_alloc_rsrv_mem(aligned_size);
    printf("Allocated memory\n");
    printf("%x\n", (int *)address);
    int id;
    for (int i = 0; i < n; i++)
    {
        ocall_print_string("Enter Class ID\n");
        ocall_enter_int(&id);
        while (find_class(id) != NULL)
        {
            ocall_print_string("ID already taken, enter another\n");
            ocall_enter_int(&id);
        }
        printf("id: %d \n", id);
        (address + i)->id = id;
        printf("id assigned\n");
        ocall_print_string("Enter class name\n");
        ocall_enter_string((address + i)->name);
        ocall_print_string("Enter professor first name\n");
        ocall_enter_string((address + i)->professor_first);
        ocall_print_string("Enter professor last name\n");
        ocall_enter_string((address + i)->professor_last);
        ocall_print_string("Enter class description\n");
        ocall_enter_string((address + i)->description);
        ocall_print_string("Enter number of credits class is worth\n");
        ocall_enter_int(&id);
        printf("Got credits\n");
        (address + i)->credits = id;
        printf("Assigned credits\n");
        if (first_class == NULL)
        {
            printf("Entered if statement\n");
            first_class = address + i;
            current_class = address + i;
            last_class = address + i;
        }
        else if (first_class->id > id)
        {
            address->next_class = first_class;
            first_class = address;
        }
        else
        {
            printf("Entered else statement\n");
            int assigned = 0;
            current_class = first_class;
            printf("Current class assigned\n");
            while (assigned == 0)
            {
                printf("entered while loop\n");
                if (current_class->next_class == NULL)
                {
                    printf("entered if statement\n");
                    current_class->next_class = (address + i);
                    assigned = 1;
                }
                else if (current_class->next_class->id > id)
                {
                    printf("entered if statement\n");
                    (address + i)->next_class = current_class->next_class;
                    current_class->next_class = (address + i);
                    assigned = 1;
                }
                else
                {
                    current_class = current_class->next_class;
                }
            }
        }
    }
    sgx_tprotect_rsrv_mem(address, aligned_size, SGX_PROT_READ | SGX_PROT_EXEC);
    return 0;
}

void print_student(Student *address)
{
    printf("Student ID: %d\n", address->id);
    printf("Name: %s %s\n", address->first_name, address->first_name);
    printf("GPA: %f\n", address->gpa);
    Personal_Class *current = address->first_class;
    printf("Classes:\n");
    while (current != NULL)
    {
        printf("%d:\n", current->current_class->id);
        printf("\tName: %s\n", current->current_class->name);
        printf("\tGrade: %d\n", current->percentage);
        printf("\tProfessor Comments: %s\n", current->comments);
        current = current->next_class;
    }
}

void print_all_students()
{
    current_student = first_student;
    while (current_student != NULL)
    {
        print_student(current_student);
        current_student = current_student->next_student;
    }
}
int public_print_student(int id)
{
    Student *address = find_student(id);
    if (address == NULL)
    {
        printf("Student not found\n");
    }
    else
    {
        print_student(address);
    }
    return 0;
}
void print_class(Class *address){
    printf("Student ID: %d\n", address->id);
    printf("Name: %s\n", address->name);
    printf("Professor: %s %s\n", address->professor_first, address->professor_last);
    printf("Description: %s\n", address->description);
    printf("Credits: %d", address->credits);
}
int print_all_classes()
{
    current_class = first_class;
    while (current_class != NULL)
    {
        print_class(current_class);
        current_class = current_class->next_class;
    }
    return 0;
}
int public_print_class(int id)
{
    Class *address = find_class(id);
    if (address == NULL)
    {
        printf("Class not found\n");
    }
    else
    {
        print_class(address);
    }
    return 0;
}
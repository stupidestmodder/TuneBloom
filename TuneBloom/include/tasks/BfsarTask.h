#pragma once

#include <framework/seadTask.h>

class BfsarTask : public sead::Task
{
public:
    explicit BfsarTask(const sead::TaskConstructArg& arg);
    ~BfsarTask() override;

    void prepare() override;
    void enter() override;
    void exit() override;
    void calc() override;
    void draw() override;

private:
};

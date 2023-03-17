/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef __ShellPlot_hh__
#define __ShellPlot_hh__

#include <ShellTask.hh>

namespace MultiShell {

  class PlotTask : public Task {
  private:
    ItemOwner<ShellBuffer>  m_owner_ds;

    int   m_x_max;
    int   m_scale;
    int   m_row;
    int   m_row_final;
    int   m_col;
    char  m_tmp[5];
  public:
    PlotTask() {
      // ...
    }
    virtual ~PlotTask();

    inline void push(ShellBuffer& dataset) {
      m_owner_ds.push(dataset);
    }
    void prepare();
  private:
    inline void finish() {
      while (m_owner_ds.count())
	m_owner_ds.pop_and_return();
    }
  public:
    virtual bool process_task(ShellStream& stream, int& afw); // returns true on completion of task
  };

  class PlotDemo : public OptionList {
  private:
    Option m_0;
    Option m_1;
    Option m_2;
  public:
    PlotDemo() :
      OptionList("Plot Demo"),
      m_0("0 Single line"),
      m_1("1 Two lines"),
      m_2("2 Oscillation")
    {
      add(m_0);
      add(m_1);
      add(m_2);
    }
  };

  class ShellPlot {
  private:
    PlotDemo  m_options;
    PlotTask  m_task;
    TaskOwner<PlotTask>  m_task_owner;
  public:
    PlotTask *task() {
      return m_task_owner.pop();
    }

    ShellPlot() {
      m_task_owner.push(m_task, true);
    }
    ~ShellPlot() {
      // ...
    }

    void demo(int option, Shell& origin, Timer *timer);
  };

} // MultiShell

#endif /* !__ShellPlot_hh__ */

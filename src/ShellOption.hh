/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef __ShellOption_hh__
#define __ShellOption_hh__

#include <ShellUtils.hh>

namespace MultiShell {

  class Option : public PrintableItem {
  private:
    const char *m_description;
  public:
    inline const char *description() const { return m_description; }

    Option(const char *desc) : m_description(desc) {
      // ...
    }
    virtual ~Option();

    /* PrintableItem:
     */
    virtual int printable_count() const;
    virtual const char *printable(int index, int& offset) const;
  };

  class OptionList : public PrintableList {
  private:
    const char *m_description;
    int  m_selection;
  public:
    OptionList(const char *description) :
      m_description(description),
      m_selection(-1)
    {
      // ...
    }
    virtual ~OptionList();

    inline const char *description() const { return m_description; }

    inline const Option *operator[](int index) const {
      return (const Option *) item(index);
    }

    inline void add(Option& option) {
      push(option);
    }

    inline const Option *current() const {
      const Option *optr = 0;

      if (m_selection >= 0) {
	optr = (const Option *) item(m_selection);
      }
      return optr;
    }

    inline void select(int selection) {
      if (selection >= 0 && selection < count()) {
	m_selection = selection;
      }
    }

    /* PrintableList:
     */
    virtual int printable_count() const;
    virtual const char *printable(int index, int& offset) const;
    virtual int selection() const;
  };

} // MultiShell

#endif /* !__ShellOption_hh__ */

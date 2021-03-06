/**********************************************************************************
    begin                : Die Sep 16 2003
    copyright            : (C) 2003 by Jeroen Wijnhout (wijnhout@science.uva.nl)
                               2008 by Michel Ludwig (michel.ludwig@kdemail.net)
 **********************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OUTPUTINFO_H
#define OUTPUTINFO_H

#include <QList>
#include <QMetaType>
#include <QString>

/**
 * Class for output-information of third program (e.g. Latex-Output, C-Compiler output)
 *
 * @author Thorsten Lck
 * @author Jeroen Wijnhout
 **/

class OutputInfo
{
	public:
		/**
		 * Constructs an invalid output information object.
		 **/
		OutputInfo();

		OutputInfo(const QString& strSrcFile, int nSrcLine, int nOutputLine, const QString& strError = QString(), int nErrorID = -1);

	public:
		/**
		 * Returns true if and only if this object contains valid output
		 * information.
		 **/
		bool isValid() const;

		/** Source file where error occurred. */
		QString source() const { return m_strSrcFile; }
		/** Source file where error occurred. */
		void setSource(const QString& src) { m_strSrcFile = src; }

		/** Line number in source file of the current message */
		int sourceLine() const { return m_nSrcLine; }
		/** Line number in source file of the current message */
		void setSourceLine(int line) { m_nSrcLine =  line; }

		/** Error message */
		QString message() const { return m_strError; }
		/** Error message */
		void setMessage(const QString& message) { m_strError = message; }

		/** Error code */
		int type() const { return m_nErrorID; }
		/** Error code */
		void setType(int type) { m_nErrorID = type; }

		/** Line number in the output, where error was reported. */
		int outputLine() const { return m_nOutputLine; }
		/** Line number in the output, where error was reported. */
		void setOutputLine(int line) { m_nOutputLine = line; }

		/**
		 * Clears the information stored in this object, turning it
		 * into an invalid output information object.
		 **/
		void clear();

		/**
		 * Comparison operator
		 **/
		bool operator==(const OutputInfo& info) const;

private:
	QString m_strSrcFile;
	int m_nSrcLine;
	QString m_strError;
	int m_nOutputLine;
	int m_nErrorID;
};

Q_DECLARE_METATYPE(OutputInfo)

/**
 * Array of OutputInfo
 * @author Thorsten Lck
 **/
typedef QList<OutputInfo> OutputInfoArray;

class LatexOutputInfo : public OutputInfo
{
	public:
		LatexOutputInfo();
		LatexOutputInfo(const QString& strSrcFile, int nSrcLine, int nOutputLine, const QString& strError = QString(), int nErrorID = -1);
	
	public:
		/**
		 * These constants are describing, which item types is currently
		 * parsed. (to be set as error code)
		 **/
		enum tagCookies
		{
			itmNone = 0,
			itmError,
			itmWarning,
			itmBadBox
		};
};

/**
 * Array of LatexOutputInfo
 * @author Thorsten Lck
 **/
typedef QList<LatexOutputInfo> LatexOutputInfoArray;

#endif

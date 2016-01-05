/*
 *   File name: ProcessOutput.h
 *   Summary:	Terminal-like window to watch output of an external process
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#ifndef ProcessOutput_h
#define ProcessOutput_h

#include <QDialog>
#include <QProcess>
#include <QList>
#include <QTextStream>
#include <QStringList>

#include "ui_process-output.h"

class QCloseEvent;


/**
 * Terminal-like window to watch output of external processes started via
 * QProcess. The command invoked by the process, its stdout and stderr output
 * are displayed in different colors.
 *
 * This class can watch more than one process: It can watch a sequence of
 * processes, such as QDirStat cleanup actions as they are invoked for each
 * selected item one after another, or even multiple processes running in
 * parallel (which may make the output a bit messy, of course).
 *
 * If this dialog is created, but now shown, it will (by default) show itself
 * as soon as there is any output on stderr.
 **/
class ProcessOutput: public QDialog
{
    Q_OBJECT

public:

    /**
     * Constructor.
     **/
    ProcessOutput( QWidget * parent );

    /**
     * Destructor.
     **/
    virtual ~ProcessOutput();

    /**
     * Add a process to watch. Ownership of the process is transferred to this
     * object.
     **/
    void addProcess( QProcess * process );

    /**
     * Tell this dialog that no more processes will be added, so when the last
     * one is finished and the "auto close" checkbox is checked, it may close
     * itself.
     **/
    void noMoreProcesses() { _noMoreProcesses = true; }

    /**
     * Return 'true' if this dialog closes itself automatically after the last
     * process finished successfully.
     **/
    bool autoClose() const;

    /**
     * Set if this dialog should close itself automatically after the last
     * process finished successfully.
     **/
    void setAutoClose( bool autoClose );

    /**
     * Set if this dialog should show itself if there is any output on
     * stderr. The default is 'true'.
     *
     * This means an application can create the dialog and leave it hidden, and
     * if there is any error output, it will automatically show itself -- with
     * all previous output of the watched processes on stdout and stderr.
     * If the user closes the dialog, however, it will remain closed.
     **/
    void setShowOnStderr( bool show ) { _showOnStderr = show; }

    /**
     * Return 'true' if this dialog shows itself if there is any output on
     * stderr.
     **/
    bool showOnStderr() const { return _showOnStderr; }

    /**
     * Return the text color for commands in the terminal area.
     **/
    QColor commandTextColor() const { return _commandTextColor; }

    /**
     * Set the text color for commands in the terminal area.
     **/
    void setCommandTextColor( const QColor & newColor )
	{ _commandTextColor = newColor; }

    /**
     * Return the text color for stdout output in the terminal area.
     **/
    QColor stdoutColor() const { return _stdoutColor; }

    /**
     * Set the text color for stdout output in the terminal area.
     **/
    void setStdoutColor( const QColor & newColor )
	{ _stdoutColor = newColor; }

    /**
     * Return the text color for stderr output in the terminal area.
     **/
    QColor stderrColor() const { return _stderrColor; }

    /**
     * Set the text color for stderr output in the terminal area.
     **/
    void setStderrColor( const QColor & newColor )
	{ _stderrColor = newColor; }

#if 0
    /**
     * Return the background color of the terminal area.
     **/
    QColor terminalBackground() const { return _terminalBackground; }

    /**
     * Set the background color of the terminal area.
     **/
    void setTerminalBackground( const QColor & newColor );
#endif

    /**
     * Return the internal process list.
     **/
    const QList<QProcess *> & processList() const { return _processList; }

    /**
     * Return 'true' if any process in the internal process is still active.
     **/
    bool hasActiveProcess() const;


public slots:

    /**
     * Add a command line to show in the output area.
     * This is typically displayed in white.
     **/
    void addCommandLine( const QString commandline );

    /**
     * Add one or more lines of stdout to show in the output area.
     * This is typically displayed in amber.
     **/
    void addStdout( const QString output );

    /**
     * Add one or more lines of stderr to show in the output area.
     * This is typically displayed in red.
     **/
    void addStderr( const QString output );

    /**
     * Kill all processes this class watches.
     **/
    void killAll();

    /**
     * Clear the output area, i.e. remove all previous output and commands.
     **/
    void clearOutput();


protected slots:

    /**
     * Read output on one of the watched process's stdout channel.
     **/
    void readStdout();

    /**
     * Read output on one of the watched process's stderr channel.
     **/
    void readStderr();

    /**
     * One of the watched processes finished.
     **/
    void processFinished( int exitCode, QProcess::ExitStatus exitStatus );

    /**
     * One of the watched processes terminated with an error.
     **/
    void processError( QProcess::ProcessError error );

    /**
     * Zoom the output area in, i.e. make its font larger.
     **/
    void zoomIn();

    /**
     * Zoom the output area out, i.e. make its font smaller.
     **/
    void zoomOut();

    /**
     * Reset the output area zoom, i.e. restore its default font.
     **/
    void resetZoom();


protected:

    /**
     * Close event: Invoked upon QDialog::close(), i.e. the "Close" button, the
     * window manager close button (the [x] at the top right), or when this
     * dialog decides to auto-close itself after the last process finishes
     * successfully.
     *
     * This object will delete itself in this event if there are no more
     * processes to watch.
     *
     * Reimplemented from QDialog / QWidget.
     **/
    void closeEvent( QCloseEvent * event ) Q_DECL_OVERRIDE;

    /**
     * Add one or more lines of text in text color 'textColor' to the output
     * area.
     **/
    void addText( const QString & text, const QColor & textColor );

    /**
     * Obtain the process to use from sender(). Return 0 if this is not a
     * QProcess.
     **/
    QProcess * senderProcess( const char * callingFunctionName ) const;

    /**
     * Zoom the terminal font by the specified factor.
     **/
    void zoom( double factor = 1.0 );


    //
    // Data members
    //

    Ui::ProcessOutputDialog	* _ui;
    QList<QProcess *>		  _processList;
    bool			  _showOnStderr;
    bool			  _noMoreProcesses;
    bool			  _closed;
    QColor			  _terminalBackground;
    QColor			  _commandTextColor;
    QColor			  _stdoutColor;
    QColor			  _stderrColor;
    QFont			  _terminalDefaultFont;

};	// class ProcessOutput


inline QTextStream & operator<< ( QTextStream & stream, QProcess * process )
{
    if ( process )
    {
	// The common case is to start an external command with
	//    /bin/sh -c theRealCommand arg1 arg2 arg3 ...
	QStringList args = process->arguments();

	if ( ! args.isEmpty() )
	    args.removeFirst();		  // Remove the "-c"

	if ( args.isEmpty() )		  // Nothing left?
	    stream << process->program(); // Ok, use the program name
	else
	    stream << args.join( " " );	  // output only the real command and its args
    }
    else
    {
	stream << "<NULL QProcess>";
    }

    return stream;
}



#endif	// ProcessOutput_h
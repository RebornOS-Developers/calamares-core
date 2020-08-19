/* === This file is part of Calamares - <https://github.com/calamares> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *
 *   Calamares is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Calamares is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Calamares. If not, see <http://www.gnu.org/licenses/>.
 *
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *   License-Filename: LICENSE
 *
 */

#include "JobQueue.h"

#include "CalamaresConfig.h"
#include "GlobalStorage.h"
#include "Job.h"
#include "utils/Logger.h"

#include <QMutex>
#include <QMutexLocker>
#include <QThread>

namespace Calamares
{

struct WeightedJob
{
    /** @brief Cumulative weight **before** this job starts
     *
     * This is calculated as jobs come in.
     */
    qreal cumulative = 0.0;
    /** @brief Weight of the job within the module's jobs
     *
     * When a list of jobs is added from a particular module,
     * the jobs are weighted relative to that module's overall weight
     * **and** the other jobs in the list, so that each job
     * gets its share:
     *      ( job-weight / total-job-weight ) * module-weight
     */
    qreal weight = 0.0;

    job_ptr job;
};
using WeightedJobList = QList< WeightedJob >;

class JobThread : public QThread
{
public:
    JobThread( JobQueue* queue )
        : QThread( queue )
        , m_queue( queue )
        , m_jobIndex( 0 )
    {
    }

    virtual ~JobThread() override;

    void finalize()
    {
        Q_ASSERT( m_runningJobs->isEmpty() );
        QMutexLocker qlock( &m_enqueMutex );
        QMutexLocker rlock( &m_runMutex );
        std::swap( m_runningJobs, m_queuedJobs );
        m_overallQueueWeight
            = m_runningJobs->isEmpty() ? 0.0 : ( m_runningJobs->last().cumulative + m_runningJobs->last().weight );
        if ( m_overallQueueWeight < 1 )
        {
            m_overallQueueWeight = 1.0;
        }
    }

    void enqueue( int moduleWeight, const JobList& jobs )
    {
        QMutexLocker qlock( &m_enqueMutex );

        qreal cumulative
            = m_queuedJobs->isEmpty() ? 0.0 : ( m_queuedJobs->last().cumulative + m_queuedJobs->last().weight );

        qreal totalJobWeight
            = std::accumulate( jobs.cbegin(), jobs.cend(), qreal( 0.0 ), []( qreal total, const job_ptr& j ) {
                  return total + j->getJobWeight();
              } );
        if ( totalJobWeight < 1 )
        {
            totalJobWeight = 1.0;
        }

        for ( const auto& j : jobs )
        {
            qreal jobContribution = ( j->getJobWeight() / totalJobWeight ) * moduleWeight;
            m_queuedJobs->append( WeightedJob { cumulative, jobContribution, j } );
            cumulative += jobContribution;
        }
    }

    void run() override
    {
        QMutexLocker rlock( &m_runMutex );
        bool failureEncountered = false;
        QString message;  ///< Filled in with errors
        QString details;

        m_jobIndex = 0;
        for ( const auto& jobitem : *m_runningJobs )
        {
            if ( failureEncountered && !jobitem.job->isEmergency() )
            {
                cDebug() << "Skipping non-emergency job" << jobitem.job->prettyName();
            }
            else
            {
                emitProgress( 0.0 );  // 0% for *this job*
                cDebug() << "Starting" << ( failureEncountered ? "EMERGENCY JOB" : "job" ) << jobitem.job->prettyName()
                         << '(' << ( m_jobIndex + 1 ) << '/' << m_runningJobs->count() << ')';
                connect( jobitem.job.data(), &Job::progress, this, &JobThread::emitProgress );
                auto result = jobitem.job->exec();
                if ( !failureEncountered && !result )
                {
                    // so this is the first failure
                    failureEncountered = true;
                    message = result.message();
                    details = result.details();
                }
                emitProgress( 1.0 );  // 100% for *this job*
            }
            m_jobIndex++;
        }
        if ( failureEncountered )
        {
            QMetaObject::invokeMethod(
                m_queue, "failed", Qt::QueuedConnection, Q_ARG( QString, message ), Q_ARG( QString, details ) );
        }
        else
        {
            emitProgress( 1.0 );
        }
        QMetaObject::invokeMethod( m_queue, "finish", Qt::QueuedConnection );
    }

    void emitProgress( qreal percentage ) const
    {
        percentage = qBound( 0.0, percentage, 1.0 );

        QString message;
        qreal progress = 0.0;
        if ( m_jobIndex < m_runningJobs->count() )
        {

            const auto& jobitem = m_runningJobs->at( m_jobIndex );
            progress = ( jobitem.cumulative + jobitem.weight * percentage ) / m_overallQueueWeight;
            message = jobitem.job->prettyStatusMessage();
        }
        else
        {
            progress = 1.0;
            message = tr( "Done" );
        }
        QMetaObject::invokeMethod(
            m_queue, "progress", Qt::QueuedConnection, Q_ARG( qreal, progress ), Q_ARG( QString, message ) );
    }


private:
    QMutex m_runMutex;
    QMutex m_enqueMutex;

    std::unique_ptr< WeightedJobList > m_runningJobs = std::make_unique< WeightedJobList >();
    std::unique_ptr< WeightedJobList > m_queuedJobs = std::make_unique< WeightedJobList >();

    JobQueue* m_queue;
    int m_jobIndex = 0;  ///< Index into m_runningJobs
    qreal m_overallQueueWeight = 0.0;  ///< cumulation when **all** the jobs are done
};

JobThread::~JobThread() {}


JobQueue* JobQueue::s_instance = nullptr;

JobQueue*
JobQueue::instance()
{
    return s_instance;
}


JobQueue::JobQueue( QObject* parent )
    : QObject( parent )
    , m_thread( new JobThread( this ) )
    , m_storage( new GlobalStorage( this ) )
{
    Q_ASSERT( !s_instance );
    s_instance = this;
}


JobQueue::~JobQueue()
{
    if ( m_thread->isRunning() )
    {
        m_thread->terminate();
        if ( !m_thread->wait( 300 ) )
        {
            cError() << "Could not terminate job thread (expect a crash now).";
        }
        delete m_thread;
    }

    delete m_storage;
}


void
JobQueue::start()
{
    Q_ASSERT( !m_thread->isRunning() );
    m_thread->finalize();
    m_finished = false;
    m_thread->start();
}


void
JobQueue::enqueue( int moduleWeight, const JobList& jobs )
{
    Q_ASSERT( !m_thread->isRunning() );
    m_thread->enqueue( moduleWeight, jobs );
    emit queueChanged( jobs );  // FIXME: bogus
}

void
JobQueue::finish()
{
    m_finished = true;
    emit finished();
}

GlobalStorage*
JobQueue::globalStorage() const
{
    return m_storage;
}

}  // namespace Calamares

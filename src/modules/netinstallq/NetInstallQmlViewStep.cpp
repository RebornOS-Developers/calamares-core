/*
 *   SPDX-FileCopyrightText: 2016 Luca Giambonini <almack@chakraos.org>
 *   SPDX-FileCopyrightText: 2016 Lisa Vitolo <shainer@chakraos.org>
 *   SPDX-FileCopyrightText: 2017 Kyle Robbertze  <krobbertze@gmail.com>
 *   SPDX-FileCopyrightText: 2017-2018 2020, Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2021 shiva.patt <shivanandvp@rebornos.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "NetInstallQmlViewStep.h"

#include "Config.h"
#include "GlobalStorage.h"
#include "JobQueue.h"
#include "locale/TranslatableConfiguration.h"
#include "utils/CalamaresUtilsSystem.h"
#include "utils/Logger.h"
#include "utils/Variant.h"

CALAMARES_PLUGIN_FACTORY_DEFINITION( NetInstallQmlViewStepFactory, registerPlugin< NetInstallQmlViewStep >(); )

NetInstallQmlViewStep::NetInstallQmlViewStep( QObject* parent )
    : Calamares::QmlViewStep( parent )
    , m_config( new Config( this ) )
    , m_stepName( nullptr )
{
    emit nextStatusChanged( false );
    connect( m_config, &Config::nextStatusChanged, this, &NetInstallQmlViewStep::nextStatusChanged );
}

void
NetInstallQmlViewStep::onActivate()
{
    cDebug() << "Activated " << prettyName() << "...";
    // TODO
    // m_config->updateDisplayedData();
    // cDebug() << "Refreshed QML data after activating " << prettyName() << "...";
}

QString
NetInstallQmlViewStep::prettyName() const
{
    return m_stepName ? m_stepName->get() : tr( "Netinstall Packages" );
}

QString
NetInstallQmlViewStep::prettyStatus() const
{
    return m_config->status();
}

bool
NetInstallQmlViewStep::isNextEnabled() const
{
    return !m_config->required();
}


bool
NetInstallQmlViewStep::isBackEnabled() const
{
    return true;
}


bool
NetInstallQmlViewStep::isAtBeginning() const
{
    return true;
}


bool
NetInstallQmlViewStep::isAtEnd() const
{
    return true;
}


Calamares::JobList
NetInstallQmlViewStep::jobs() const
{
    return Calamares::JobList();
}

void
NetInstallQmlViewStep::onLeave()
{
    m_config->finalizeGlobalStorage( moduleInstanceKey() );
}

void
NetInstallQmlViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );

    if ( !this->getConfig() ) {
        cDebug() << "Error: Config not valid!";
    }

    Calamares::QmlViewStep::setConfigurationMap( configurationMap );
}

void
NetInstallQmlViewStep::nextIsReady()
{
    m_nextEnabled = true;
    emit nextStatusChanged( true );
}
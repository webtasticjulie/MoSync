/*
Copyright (C) 2011 MoSync AB

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License,
version 2, as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301, USA.
*/

/**
 * @file NotificationManager.cpp
 * @author Emma Tresanszki and Bogdan Iusco
 * @date 1 Nov 2011
 *
 * @brief  The NotificationManager manages local and push notification events
 * and dispatches them to the target notifications.
 * @platform Android, iOS.
 */

// Default buffer size.
#define BUFFER_SIZE 256

#include <conprint.h>

#include "NotificationManager.h"
#include "LocalNotificationListener.h"
#include "PushNotification.h"
#include "PushNotificationListener.h"

namespace Notification
{

    /**
     * Initialize the singleton variable to NULL.
     */
    NotificationManager* NotificationManager::sInstance = NULL;

    /**
     * Constructor is protected since this is a singleton.
     * (subclasses can still create instances).
     */
    NotificationManager::NotificationManager():
        mTitle(NULL),
        mTickerText(NULL)
    {
        // Add me as a custom event listener.
        MAUtil::Environment::getEnvironment().addCustomEventListener(this);
    }

    /**
     * Destructor.
     */
    NotificationManager::~NotificationManager()
    {
		delete mTitle;
		mTitle = NULL;

		delete mTickerText;
		mTickerText = NULL;

        // Remove me as a custom event listener.
        MAUtil::Environment::getEnvironment().removeCustomEventListener(this);
    }

    /**
     * Return the single instance of this class.
     */
    NotificationManager* NotificationManager::getInstance()
    {
        if (NULL == NotificationManager::sInstance)
        {
            NotificationManager::sInstance = new NotificationManager();
        }

        return sInstance;
    }

    /**
     * Destroy the single instance of this class.
     * Call this method only when the application will exit.
     */
    void NotificationManager::destroyInstance()
    {
        delete NotificationManager::sInstance;
    }

    /**
     * Implementation of CustomEventListener interface.
     * This method will get called whenever there is a
     * event generated by the system.
     * @param event The new received event.
     */
    void NotificationManager::customEvent(const MAEvent& event)
    {
        if (EVENT_TYPE_LOCAL_NOTIFICATION == event.type)
        {
            this->receivedLocalNotification(event);
        }
        else if (EVENT_TYPE_PUSH_NOTIFICATION == event.type)
        {
            this->receivedPushNotification(event);
        }
        else if (EVENT_TYPE_PUSH_NOTIFICATION_REGISTRATION == event.type)
        {
            char buffer[BUFFER_SIZE];
            int result = maNotificationPushGetRegistration(
                buffer,
                BUFFER_SIZE);
            MAUtil::String message = buffer;
            if (MA_NOTIFICATION_RES_OK == result)
            {
                // Notify listeners.
                for (int i = 0; i < mPushNotificationListeners.size(); i++)
                {
                    PushNotificationListener* listener =
                            mPushNotificationListeners[i];
                    listener->didApplicationRegistered(message);
                }
            } else
            {
                // Notify listeners.
                for (int i = 0; i < mPushNotificationListeners.size(); i++)
                {
                    PushNotificationListener* listener =
                        mPushNotificationListeners[i];
                    listener->didFailedToRegister(message);
                }
            }
        }
        else if (EVENT_TYPE_PUSH_NOTIFICATION_UNREGISTRATION == event.type)
        {
            // This event is received only on Android.
            // Notify listeners.
            for (int i = 0; i < mPushNotificationListeners.size(); i++)
            {
                mPushNotificationListeners[i]->didApplicationUnregister();
            }
        }
    }

    /**
     * Add an event listener for local notifications.
     * @param listener The listener that will receive
     * local notification events.
     */
    void NotificationManager::addLocalNotificationListener(
        LocalNotificationListener* listener)
    {
        for (int i = 0; i < mLocalNotificationListeners.size(); i++)
        {
            if (listener == mLocalNotificationListeners[i])
            {
                return;
            }
        }

        mLocalNotificationListeners.add(listener);
    }

    /**
     * Remove the event listener for local notifications.
     * @param listener The listener that receives local notification events.
     */
    void NotificationManager::removeLocalNotificationListener(
        LocalNotificationListener* listener)
    {
        for (int i = 0; i < mLocalNotificationListeners.size(); i++)
        {
            if (listener == mLocalNotificationListeners[i])
            {
                mLocalNotificationListeners.remove(i);
                break;
            }
        }
    }

    /**
     * Schedules a local notification for delivery at its encapsulated
     * date and time.
     * By default, the notifications are displayed to the user only if the application
     * is in background. But on Android you can configure this via the
     * #MA_NOTIFICATION_LOCAL_DISPLAY_FLAG property by calling setDisplayFlag().
     * @param localNotification Handle to a local notification object.
     * @return One of the constants:
     *  - #MA_NOTIFICATION_RES_OK if no error occurred.
     *  - #MA_NOTIFICATION_RES_INVALID_HANDLE if the notificationHandle is invalid.
     *  - #MA_NOTIFICATION_RES_ALREADY_SCHEDULED if the notification was already
     *  scheduled.
     */
    int NotificationManager::scheduleLocalNotification(
        LocalNotification* localNotification)
    {
		mLocalNotificationMap.insert(localNotification->getHandle(), localNotification);
        return maNotificationLocalSchedule(localNotification->getHandle());
    }

    /**
     * Cancels the delivery of the specified scheduled local notification.
     * calling this method also programmatically dismisses the notification
     * if  it is currently displaying an alert.
     * @param localNotification Handle to a local notification object.
     * @return One of the constants:
     *  - #MA_NOTIFICATION_RES_OK
     *  - #MA_NOTIFICATION_RES_INVALID_HANDLE if the notificationHandle is invalid.
     *  - #MA_NOTIFICATION_RES_CANNOT_UNSCHEDULE If the notification was not
     *  scheduled.
     */
    int NotificationManager::unscheduleLocalNotification(
        LocalNotification* localNotification)
    {
		mLocalNotificationMap.erase(localNotification->getHandle());
        return maNotificationLocalUnschedule(localNotification->getHandle());
    }

    /**
     * Registers the current application for receiving push notifications.
     * Registration is made on the Apple Push Service, or Google GCM/C2DM Service,
     * depending on the underlying platform.
     * @param types A bit mask specifying the types of notifications the
     * application accepts.
     * See PushNotificationType for valid bit-mask values.
     * This param is applied only on iOS platform. Android platform will
     * ignore this value.
     * @param senderId Your projectId obtained from here:
     * http://developer.android.com/guide/google/gcm/gs.html#create-proj
     * For old applications, this param was set as the ID of the account
     * authorized to send messages to the application, typically the email
     * address of an account set up by the application's developer.
     * Even though setting the senderId as the accountID was deprecated, old
     * Android applications still support it.
     * On iOS platform this param is ignored.
     *
     * Example: Notification::getInstance->registerPushNotification(
     *  PUSH_NOTIFICATION_TYPE_BADGE | PUSH_NOTIFICATION_TYPE_ALERT, "yoursenderId_here");
     *
     *  @return One of the next result codes:
     *  - #MA_NOTIFICATION_RES_OK if no error occurred.
     *  - #MA_NOTIFICATION_RES_ALREADY_REGISTERED if the application is already
     *    registered for receiving push notifications.
     *  - #MA_NOTIFICATION_RES_UNSUPPORTED if notifications are not supported
     *  on current platform.
     */
    int NotificationManager::registerPushNotification(
        const int types,
        const MAUtil::String& senderId)
    {
        return maNotificationPushRegister(types, senderId.c_str());
    }

    /**
     * Unregister application for push notifications.
     */
    void NotificationManager::unregisterPushNotification()
    {
        maNotificationPushUnregister();
    }

    /**
     * Add listener for push notifications received by this application.
     * @param listener The listener that will receive
     * push notification events.
     * Don't forget to register the application for receiving push
     * notifications by calling registerPushNotification function.
     */
    void NotificationManager::addPushNotificationListener(
        PushNotificationListener* listener)
    {
        for (int i = 0; i < mPushNotificationListeners.size(); i++)
        {
            if (listener == mPushNotificationListeners[i])
            {
                return;
            }
        }

        mPushNotificationListeners.add(listener);
    }

    /**
     * Remove listener for push notifications received by this application.
     * @param listener The listener that receives push notification events.
     */
    void NotificationManager::removePushNotificationListener(
        PushNotificationListener* listener)
    {
        for (int i = 0; i < mPushNotificationListeners.size(); i++)
        {
            if (listener == mPushNotificationListeners[i])
            {
                mPushNotificationListeners.remove(i);
                break;
            }
        }
    }

    /**
     * Set the number currently set as the badge of the application icon.
     * Platform: iOS only.
     * @param iconBadgeNumber The number that will be set as the badge of
     * the application icon.
     * If this value is negative this method will do nothing.
     */
    void NotificationManager::setApplicationIconBadgeNumber(
        const int iconBadgeNumber)
    {
        maNotificationSetIconBadge(iconBadgeNumber);
    }

    /**
     * Get the number currently set as the badge of the application icon.
     * Platform: iOS only.
     * @return The number currently set as the badge of the application icon.
     */
    int NotificationManager::getApplicationIconBadgeNumber()
    {
        return maNotificationGetIconBadge();
    }

    /**
     * Set the display flags applied to the incoming push notifications.
     * Note that regardless of this setting, the didReceivePushNotification
     * callback will be made for each incoming notification.
     * #NOTIFICATION_DISPLAY_DEFAULT is enabled by default.
     * Platform: Android only.
     * @param displayFlag  is the required state of the application for
     * a notification to be displayed. One of the constants:
     *  - #NOTIFICATION_DISPLAY_DEFAULT
     *  - #NOTIFICATION_DISPLAY_ANYTIME.
     * @return Any of the following result codes:
     * - #MA_NOTIFICATION_RES_OK if the property could be set.
     * - #MA_NOTIFICATION_RES_INVALID_PROPERTY_NAME if the property name
     * was invalid for the target platform.
     */
    int NotificationManager::setPushNotificationsDisplayFlag(
		const NotificationDisplayFlag displayFlag)
    {
		return maNotificationPushSetDisplayFlag(displayFlag);
    }

    /**
     * Set the  message title in the notification area for incoming push
     * notifications.
     * This call does not alter already received notifications.
     * Platform: Android only.
     * @param title The title that goes in the expanded entry of the
     * notification.
     * @return Any of the following result codes:
     * - #MA_NOTIFICATION_RES_OK if the property could be set.
     * - #MA_NOTIFICATION_RES_INVALID_PROPERTY_NAME if the property name
     * was invalid for the target platform.
     */
    int NotificationManager::setPushNotificationsTitle(
        const MAUtil::String& title)
    {
        return maNotificationPushSetMessageTitle(title.c_str());
    }

    /**
     * Set the ticker text in the notification status bar for incoming push
     * notifications.
     * This call does not alter already received notifications.
     * Platform: Android only.
     * @param ticker The text that flows by in the status bar when the
     * notification first activates.
     * @return Any of the following result codes:
     * - #MA_NOTIFICATION_RES_OK if the property could be set.
     * - #MA_NOTIFICATION_RES_INVALID_PROPERTY_NAME if the property name
     * was invalid for the target platform.
     */
    int NotificationManager::setPushNotificationsTickerText(
        const MAUtil::String& ticker)
    {
        return maNotificationPushSetTickerText(ticker.c_str());
    }

    /**
     * Get the message title of the incoming notifications.
     * This text can be set with NotificationManager::setPushNotificationsTitle
     * to apply to all incoming notifications.
     * Platform: Android only.
     * @return the title that goes in the expanded entry of the notification.
     */
    MAUtil::String NotificationManager::getMessageTitle() const
    {
        if (mTitle)
        {
            return *mTitle;
        }
        else
        {
            return "";
        }
    }

    /**
     * Get the ticker text in the notification status bar for the incoming
     * notifications.
     * This text can be set with
     * NotificationManager::setPushNotificationsTickerText
     * to apply to all incoming notifications.
     * Platform: Android only.
     * @return The text that flows by in the status bar when the notification
     * first activates.
     */
    MAUtil::String NotificationManager::getTickerText() const
    {
        if (mTickerText)
        {
            return *mTickerText;
        }
        else
        {
            return "";
        }
    }

    /**
     * Notifies listeners that a new local notification event has been
     * received.
     * @param event The new received event.
     */
    void NotificationManager::receivedLocalNotification(const MAEvent& event)
    {
        if (EVENT_TYPE_LOCAL_NOTIFICATION == event.type)
        {
			// Check if the local notification exists in the map.
			if (mLocalNotificationMap.end()
					!= mLocalNotificationMap.find(event.localNotificationHandle))
			{
				// Get the local notification object that wraps the handle.
				LocalNotification* localNotification =
					mLocalNotificationMap[event.localNotificationHandle];

				// Notify listeners.
				for (int i = 0; i < mLocalNotificationListeners.size(); i++) {
					LocalNotificationListener* listener =
							mLocalNotificationListeners[i];
					listener->didReceiveLocalNotification(*localNotification);
				}
			}
        }
    }

    /**
     * Notifies listeners that a new push notification event has been
     * received.
     * @param event The new received event.
     */
    void NotificationManager::receivedPushNotification(const MAEvent& event)
    {
        MAHandle pushNotificationHandle = event.pushNotificationHandle;

        // Get push notification data
        MAPushNotificationData data;
        char messageAlert[BUFFER_SIZE];
        char sound[BUFFER_SIZE];
        data.alertMessage = messageAlert;
        data.alertMessageSize = BUFFER_SIZE;
        data.soundFileName = sound;
        data.soundFileNameSize = BUFFER_SIZE;
        // The default value for the type is MA_NOTIFICATION_PUSH_TYPE_ALERT
        data.type = MA_NOTIFICATION_PUSH_TYPE_ALERT;
        int result = maNotificationPushGetData(pushNotificationHandle, &data);
        if (MA_NOTIFICATION_RES_OK != result)
        {
            printf("NotificationManager::customEvent error = %d", result);
            return;
        }

        // Create the push notification object.
        PushNotification* pushNotificationObj = new PushNotification();
        if (data.type & MA_NOTIFICATION_PUSH_TYPE_ALERT)
        {
            pushNotificationObj->setMessage(data.alertMessage);
        }
        if (data.type & MA_NOTIFICATION_PUSH_TYPE_SOUND)
        {
            pushNotificationObj->setSoundFileName(data.soundFileName);
        }
        if (data.type & MA_NOTIFICATION_PUSH_TYPE_BADGE)
        {
            pushNotificationObj->setIconBadge(data.badgeIcon);
        }

        // Notify listeners.
        for (int i = 0; i < mPushNotificationListeners.size(); i++)
        {
            PushNotificationListener* listener =
                mPushNotificationListeners[i];
            listener->didReceivePushNotification(*pushNotificationObj);
        }

        delete pushNotificationObj;
        maNotificationPushDestroy(pushNotificationHandle);
    }

} // namespace Notification

/*
 *  Copyright (c) 2020, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definitions for SRP server.
 */

#ifndef NET_SRP_SERVER_HPP_
#define NET_SRP_SERVER_HPP_

#include "openthread-core-config.h"

#if OPENTHREAD_CONFIG_SRP_SERVER_ENABLE

#if !OPENTHREAD_CONFIG_TMF_NETDATA_SERVICE_ENABLE
#error "OPENTHREAD_CONFIG_TMF_NETDATA_SERVICE_ENABLE is required for OPENTHREAD_CONFIG_SRP_SERVER_ENABLE"
#endif

#if !OPENTHREAD_CONFIG_NETDATA_PUBLISHER_ENABLE
#error "OPENTHREAD_CONFIG_NETDATA_PUBLISHER_ENABLE is required for OPENTHREAD_CONFIG_SRP_SERVER_ENABLE"
#endif

#if !OPENTHREAD_CONFIG_ECDSA_ENABLE
#error "OPENTHREAD_CONFIG_ECDSA_ENABLE is required for OPENTHREAD_CONFIG_SRP_SERVER_ENABLE"
#endif

#include <openthread/ip6.h>
#include <openthread/srp_server.h>

#include "common/array.hpp"
#include "common/as_core_type.hpp"
#include "common/clearable.hpp"
#include "common/heap.hpp"
#include "common/heap_allocatable.hpp"
#include "common/heap_array.hpp"
#include "common/heap_data.hpp"
#include "common/heap_string.hpp"
#include "common/linked_list.hpp"
#include "common/locator.hpp"
#include "common/non_copyable.hpp"
#include "common/notifier.hpp"
#include "common/num_utils.hpp"
#include "common/numeric_limits.hpp"
#include "common/retain_ptr.hpp"
#include "common/timer.hpp"
#include "crypto/ecdsa.hpp"
#include "net/dns_types.hpp"
#include "net/ip6.hpp"
#include "net/ip6_address.hpp"
#include "net/udp6.hpp"
#include "thread/network_data_publisher.hpp"

struct otSrpServerHost
{
};

struct otSrpServerService
{
};

namespace ot {

namespace Dns {
namespace ServiceDiscovery {
class Server;
}
} // namespace Dns

#if OPENTHREAD_CONFIG_BORDER_ROUTING_ENABLE
namespace BorderRouter {
class RoutingManager;
}
#endif

namespace Srp {

/**
 * This class implements the SRP server.
 *
 */
class Server : public InstanceLocator, private NonCopyable
{
    friend class NetworkData::Publisher;
    friend class UpdateMetadata;
    friend class Service;
    friend class Host;
    friend class Dns::ServiceDiscovery::Server;
#if OPENTHREAD_CONFIG_BORDER_ROUTING_ENABLE
    friend class BorderRouter::RoutingManager;
#endif

    enum RetainName : bool
    {
        kDeleteName = false,
        kRetainName = true,
    };

    enum NotifyMode : bool
    {
        kDoNotNotifyServiceHandler = false,
        kNotifyServiceHandler      = true,
    };

public:
    static constexpr uint16_t kUdpPortMin = OPENTHREAD_CONFIG_SRP_SERVER_UDP_PORT_MIN; ///< The reserved min port.
    static constexpr uint16_t kUdpPortMax = OPENTHREAD_CONFIG_SRP_SERVER_UDP_PORT_MAX; ///< The reserved max port.

    static_assert(kUdpPortMin <= kUdpPortMax, "invalid port range");

    /**
     * The ID of SRP service update transaction.
     *
     */
    typedef otSrpServerServiceUpdateId ServiceUpdateId;

    /**
     * The SRP server lease information of a host/service.
     *
     */
    typedef otSrpServerLeaseInfo LeaseInfo;

    /**
     * This enumeration represents the address mode used by the SRP server.
     *
     * Address mode specifies how the address and port number are determined by the SRP server and how this info ins
     * published in the Thread Network Data.
     *
     */
    enum AddressMode : uint8_t
    {
        kAddressModeUnicast = OT_SRP_SERVER_ADDRESS_MODE_UNICAST, ///< Unicast address mode.
        kAddressModeAnycast = OT_SRP_SERVER_ADDRESS_MODE_ANYCAST, ///< Anycast address mode.
    };

    class Host;

    /**
     * This enumeration represents the state of SRP server.
     *
     */
    enum State : uint8_t
    {
        kStateDisabled = OT_SRP_SERVER_STATE_DISABLED, ///< Server is disabled.
        kStateRunning  = OT_SRP_SERVER_STATE_RUNNING,  ///< Server is enabled and running.
        kStateStopped  = OT_SRP_SERVER_STATE_STOPPED,  ///< Server is enabled but stopped.
    };

    /**
     * This class implements a server-side SRP service.
     *
     */
    class Service : public otSrpServerService,
                    public LinkedListEntry<Service>,
                    private Heap::Allocatable<Service>,
                    private NonCopyable
    {
        friend class Server;
        friend class LinkedList<Service>;
        friend class LinkedListEntry<Service>;
        friend class Heap::Allocatable<Service>;

    public:
        /**
         * This type represents the flags which indicates which services to include or exclude when searching in (or
         * iterating over) the list of SRP services.
         *
         */
        typedef otSrpServerServiceFlags Flags;

        /**
         * This `Flags` constant indicates to include base services (not a sub-type).
         *
         */
        static constexpr Flags kFlagBaseType = OT_SRP_SERVER_SERVICE_FLAG_BASE_TYPE;

        /**
         * This `Flags` constant indicates to include sub-type services.
         *
         */
        static constexpr Flags kFlagSubType = OT_SRP_SERVER_SERVICE_FLAG_SUB_TYPE;

        /**
         * This `Flags` constant indicates to include active (not deleted) services.
         *
         */
        static constexpr Flags kFlagActive = OT_SRP_SERVER_SERVICE_FLAG_ACTIVE;

        /**
         * This `Flags` constant indicates to include deleted services.
         *
         */
        static constexpr Flags kFlagDeleted = OT_SRP_SERVER_SERVICE_FLAG_DELETED;

        /**
         * This method tells if the SRP service has been deleted.
         *
         * A SRP service can be deleted but retains its name for future uses.
         * In this case, the service instance is not removed from the SRP server/registry.
         * It is guaranteed that all services are deleted if the host is deleted.
         *
         * @returns  TRUE if the service has been deleted, FALSE if not.
         *
         */
        bool IsDeleted(void) const { return mIsDeleted; }

        /**
         * This method indicates whether the SRP service is a sub-type.
         *
         * @retval TRUE    If the service is a sub-type.
         * @retval FALSE   If the service is not a sub-type.
         *
         */
        bool IsSubType(void) const { return mIsSubType; }

        /**
         * This method gets the full service instance name of the service.
         *
         * @returns  A pointer service instance name (as a null-terminated C string).
         *
         */
        const char *GetInstanceName(void) const { return mDescription->mInstanceName.AsCString(); }

        /**
         * This method gets the full service name of the service.
         *
         * @returns  A pointer service name (as a null-terminated C string).
         *
         */
        const char *GetServiceName(void) const { return mServiceName.AsCString(); }

        /**
         * This method gets the sub-type label from service name.
         *
         * The full service name for a sub-type service follows "<sub-label>._sub.<service-labels>.<domain>.". This
         * method copies the `<sub-label>` into the @p aLabel buffer.
         *
         * The @p aLabel is ensured to always be null-terminated after returning even in case of failure.
         *
         * @param[out] aLabel        A pointer to a buffer to copy the sub-type label name.
         * @param[in]  aMaxSize      Maximum size of @p aLabel buffer.
         *
         * @retval kErrorNone         @p aLabel was updated successfully.
         * @retval kErrorNoBufs       The sub-type label could not fit in @p aLabel buffer (number of chars from label
         *                            that could fit are copied in @p aLabel ensuring it is null-terminated).
         * @retval kErrorInvalidArgs  SRP service is not a sub-type.
         *
         */
        Error GetServiceSubTypeLabel(char *aLabel, uint8_t aMaxSize) const;

        /**
         * This method returns the TTL of the service instance.
         *
         * @returns The TTL of the service instance.
         *
         */
        uint32_t GetTtl(void) const { return mDescription->mTtl; }

        /**
         * This method returns the port of the service instance.
         *
         * @returns  The port of the service.
         *
         */
        uint16_t GetPort(void) const { return mDescription->mPort; }

        /**
         * This method returns the weight of the service instance.
         *
         * @returns  The weight of the service.
         *
         */
        uint16_t GetWeight(void) const { return mDescription->mWeight; }

        /**
         * This method returns the priority of the service instance.
         *
         * @returns  The priority of the service.
         *
         */
        uint16_t GetPriority(void) const { return mDescription->mPriority; }

        /**
         * This method returns the TXT record data of the service instance.
         *
         * @returns A pointer to the buffer containing the TXT record data.
         *
         */
        const uint8_t *GetTxtData(void) const { return mDescription->mTxtData.GetBytes(); }

        /**
         * This method returns the TXT record data length of the service instance.
         *
         * @return The TXT record data length (number of bytes in buffer returned from `GetTxtData()`).
         *
         */
        uint16_t GetTxtDataLength(void) const { return mDescription->mTxtData.GetLength(); }

        /**
         * This method returns the host which the service instance reside on.
         *
         * @returns  A reference to the host instance.
         *
         */
        const Host &GetHost(void) const { return *mDescription->mHost; }

        /**
         * This method returns the LEASE time of the service.
         *
         * @returns  The LEASE time in seconds.
         *
         */
        uint32_t GetLease(void) const { return mDescription->mLease; }

        /**
         * This method returns the KEY-LEASE time of the key of the service.
         *
         * @returns  The KEY-LEASE time in seconds.
         *
         */
        uint32_t GetKeyLease(void) const { return mDescription->mKeyLease; }

        /**
         * This method returns the expire time (in milliseconds) of the service.
         *
         * @returns  The service expire time in milliseconds.
         *
         */
        TimeMilli GetExpireTime(void) const;

        /**
         * This method returns the key expire time (in milliseconds) of the service.
         *
         * @returns  The service key expire time in milliseconds.
         *
         */
        TimeMilli GetKeyExpireTime(void) const;

        /**
         * This method gets the LEASE and KEY-LEASE information of a given service.
         *
         * @param[out]  aLeaseInfo  A reference to a LeaseInfo instance. It contains the LEASE time, KEY-LEASE time,
         *                          remaining LEASE time and the remaining KEY-LEASE time.
         *
         */
        void GetLeaseInfo(LeaseInfo &aLeaseInfo) const;

        /**
         * This method indicates whether this service matches a given service instance name.
         *
         * @param[in]  aInstanceName  The service instance name.
         *
         * @retval  TRUE   If the service matches the service instance name.
         * @retval  FALSE  If the service does not match the service instance name.
         *
         */
        bool MatchesInstanceName(const char *aInstanceName) const;

        /**
         * This method tells whether this service matches a given service name.
         *
         * @param[in] aServiceName  The full service name to match.
         *
         * @retval  TRUE   If the service matches the full service name.
         * @retval  FALSE  If the service does not match the full service name.
         *
         */
        bool MatchesServiceName(const char *aServiceName) const;

    private:
        struct Description : public LinkedListEntry<Description>,
                             public Heap::Allocatable<Description>,
                             public RetainCountable,
                             private NonCopyable
        {
            Error       Init(const char *aInstanceName, Host &aHost);
            const char *GetInstanceName(void) const { return mInstanceName.AsCString(); }
            bool        Matches(const char *aInstanceName) const;
            void        ClearResources(void);
            void        TakeResourcesFrom(Description &aDescription);
            Error       SetTxtDataFromMessage(const Message &aMessage, uint16_t aOffset, uint16_t aLength);

            Description *mNext;
            Heap::String mInstanceName;
            Host        *mHost;
            Heap::Data   mTxtData;
            uint16_t     mPriority;
            uint16_t     mWeight;
            uint16_t     mPort;
            uint32_t     mTtl;      // The TTL in seconds.
            uint32_t     mLease;    // The LEASE time in seconds.
            uint32_t     mKeyLease; // The KEY-LEASE time in seconds.
            TimeMilli    mUpdateTime;
        };

        enum Action : uint8_t
        {
            kAddNew,
            kUpdateExisting,
            kRemoveButRetainName,
            kFullyRemove,
            kLeaseExpired,
            kKeyLeaseExpired,
        };

        Error Init(const char *aServiceName, Description &aDescription, bool aIsSubType, TimeMilli aUpdateTime);
        bool  MatchesFlags(Flags aFlags) const;
        const TimeMilli &GetUpdateTime(void) const { return mUpdateTime; }
        void             Log(Action aAction) const;

        Heap::String           mServiceName;
        RetainPtr<Description> mDescription;
        Service               *mNext;
        TimeMilli              mUpdateTime;
        bool                   mIsDeleted : 1;
        bool                   mIsSubType : 1;
        bool                   mIsCommitted : 1;
    };

    /**
     * This class implements the Host which registers services on the SRP server.
     *
     */
    class Host : public otSrpServerHost,
                 public InstanceLocator,
                 public LinkedListEntry<Host>,
                 private Heap::Allocatable<Host>,
                 private NonCopyable
    {
        friend class Server;
        friend class LinkedListEntry<Host>;
        friend class Heap::Allocatable<Host>;

    public:
        /**
         * This method tells whether the Host object has been deleted.
         *
         * The Host object retains event if the host has been deleted by the SRP client,
         * because the host name may retain.
         *
         * @returns  TRUE if the host is deleted, FALSE if the host is not deleted.
         *
         */
        bool IsDeleted(void) const { return (mLease == 0); }

        /**
         * This method returns the full name of the host.
         *
         * @returns  A pointer to the null-terminated full host name.
         *
         */
        const char *GetFullName(void) const { return mFullName.AsCString(); }

        /**
         * This method returns addresses of the host.
         *
         * @param[out]  aAddressesNum  The number of the addresses.
         *
         * @returns  A pointer to the addresses array or `nullptr` if no addresses.
         *
         */
        const Ip6::Address *GetAddresses(uint8_t &aAddressesNum) const
        {
            aAddressesNum = ClampToUint8(mAddresses.GetLength());

            return mAddresses.AsCArray();
        }

        /**
         * This method returns the TTL of the host.
         *
         * @returns The TTL of the host.
         *
         */
        uint32_t GetTtl(void) const { return mTtl; }

        /**
         * This method returns the LEASE time of the host.
         *
         * @returns  The LEASE time in seconds.
         *
         */
        uint32_t GetLease(void) const { return mLease; }

        /**
         * This method returns the KEY-LEASE time of the key of the host.
         *
         * @returns  The KEY-LEASE time in seconds.
         *
         */
        uint32_t GetKeyLease(void) const { return mKeyLease; }

        /**
         * This method gets the LEASE and KEY-LEASE information of a given host.
         *
         * @param[out]  aLeaseInfo  A reference to a LeaseInfo instance. It contains the LEASE time, KEY-LEASE time,
         *                          remaining LEASE time and the remaining KEY-LEASE time.
         *
         */
        void GetLeaseInfo(LeaseInfo &aLeaseInfo) const;

        /**
         * This method returns the KEY resource record of the host.
         *
         * @returns  A pointer to the ECDSA P 256 public key resource record
         *           if there is valid one. `nullptr` if no valid key exists.
         *
         */
        const Dns::Ecdsa256KeyRecord *GetKeyRecord(void) const { return mKeyRecord.IsValid() ? &mKeyRecord : nullptr; }

        /**
         * This method returns the expire time (in milliseconds) of the host.
         *
         * @returns  The expire time in milliseconds.
         *
         */
        TimeMilli GetExpireTime(void) const;

        /**
         * This method returns the expire time (in milliseconds) of the key of the host.
         *
         * @returns  The expire time of the key in milliseconds.
         *
         */
        TimeMilli GetKeyExpireTime(void) const;

        /**
         * This method returns the `Service` linked list associated with the host.
         *
         * @returns The `Service` linked list.
         *
         */
        const LinkedList<Service> &GetServices(void) const { return mServices; }

        /**
         * This method finds the next matching service on the host.
         *
         * @param[in] aPrevService   A pointer to the previous service or `nullptr` to start from beginning of the list.
         * @param[in] aFlags         Flags indicating which services to include (base/sub-type, active/deleted).
         * @param[in] aServiceName   The service name to match. Set to `nullptr` to accept any name.
         * @param[in] aInstanceName  The service instance name to match. Set to `nullptr` to accept any name.
         *
         * @returns  A pointer to the next matching service or `nullptr` if no matching service could be found.
         *
         */
        const Service *FindNextService(const Service *aPrevService,
                                       Service::Flags aFlags        = kFlagsAnyService,
                                       const char    *aServiceName  = nullptr,
                                       const char    *aInstanceName = nullptr) const;

        /**
         * This method tells whether the host matches a given full name.
         *
         * @param[in]  aFullName  The full name.
         *
         * @returns  A boolean that indicates whether the host matches the given name.
         *
         */
        bool Matches(const char *aFullName) const;

    private:
        Host(Instance &aInstance, TimeMilli aUpdateTime);
        ~Host(void);

        Error SetFullName(const char *aFullName);
        void  SetKeyRecord(Dns::Ecdsa256KeyRecord &aKeyRecord);
        void  SetTtl(uint32_t aTtl) { mTtl = aTtl; }
        void  SetLease(uint32_t aLease) { mLease = aLease; }
        void  SetKeyLease(uint32_t aKeyLease) { mKeyLease = aKeyLease; }
        Error ProcessTtl(uint32_t aTtl);

        LinkedList<Service> &GetServices(void) { return mServices; }
        Service             *AddNewService(const char *aServiceName,
                                           const char *aInstanceName,
                                           bool        aIsSubType,
                                           TimeMilli   aUpdateTime);
        void                 RemoveService(Service *aService, RetainName aRetainName, NotifyMode aNotifyServiceHandler);
        Error                AddCopyOfServiceAsDeletedIfNotPresent(const Service &aService, TimeMilli aUpdateTime);
        void                 FreeAllServices(void);
        void                 ClearResources(void);
        Error                MergeServicesAndResourcesFrom(Host &aHost);
        Error                AddIp6Address(const Ip6::Address &aIp6Address);
        bool                 HasServiceInstance(const char *aInstanceName) const;
        RetainPtr<Service::Description>       FindServiceDescription(const char *aInstanceName);
        const RetainPtr<Service::Description> FindServiceDescription(const char *aInstanceName) const;
        Service                              *FindService(const char *aServiceName, const char *aInstanceName);
        const Service                        *FindService(const char *aServiceName, const char *aInstanceName) const;
        Service                              *FindBaseService(const char *aInstanceName);
        const Service                        *FindBaseService(const char *aInstanceName) const;

        Host                     *mNext;
        Heap::String              mFullName;
        Heap::Array<Ip6::Address> mAddresses;

        // TODO(wgtdkp): there is no necessary to save the entire resource
        // record, saving only the ECDSA-256 public key should be enough.
        Dns::Ecdsa256KeyRecord mKeyRecord;
        uint32_t               mTtl;      // The TTL in seconds.
        uint32_t               mLease;    // The LEASE time in seconds.
        uint32_t               mKeyLease; // The KEY-LEASE time in seconds.
        TimeMilli              mUpdateTime;
        LinkedList<Service>    mServices;
    };

    /**
     * This class handles TTL configuration.
     *
     */
    class TtlConfig : public otSrpServerTtlConfig
    {
        friend class Server;

    public:
        /**
         * This constructor initializes to default TTL configuration.
         *
         */
        TtlConfig(void);

    private:
        bool     IsValid(void) const { return mMinTtl <= mMaxTtl; }
        uint32_t GrantTtl(uint32_t aLease, uint32_t aTtl) const;
    };

    /**
     * This class handles LEASE and KEY-LEASE configurations.
     *
     */
    class LeaseConfig : public otSrpServerLeaseConfig
    {
        friend class Server;

    public:
        /**
         * This constructor initialize to default LEASE and KEY-LEASE configurations.
         *
         */
        LeaseConfig(void);

    private:
        bool     IsValid(void) const;
        uint32_t GrantLease(uint32_t aLease) const;
        uint32_t GrantKeyLease(uint32_t aKeyLease) const;
    };

    /**
     * This constant defines a `Service::Flags` combination accepting any service (base/sub-type, active/deleted).
     *
     */
    static constexpr Service::Flags kFlagsAnyService = OT_SRP_SERVER_FLAGS_ANY_SERVICE;

    /**
     * This constant defines a `Service::Flags` combination accepting base services only.
     *
     */
    static constexpr Service::Flags kFlagsBaseTypeServiceOnly = OT_SRP_SERVER_FLAGS_BASE_TYPE_SERVICE_ONLY;

    /**
     * This constant defines a `Service::Flags` combination accepting sub-type services only.
     *
     */
    static constexpr Service::Flags kFlagsSubTypeServiceOnly = OT_SRP_SERVER_FLAGS_SUB_TYPE_SERVICE_ONLY;

    /**
     * This constant defines a `Service::Flags` combination accepting any active services (not deleted).
     *
     */
    static constexpr Service::Flags kFlagsAnyTypeActiveService = OT_SRP_SERVER_FLAGS_ANY_TYPE_ACTIVE_SERVICE;

    /**
     * This constant defines a `Service::Flags` combination accepting any deleted services.
     *
     */
    static constexpr Service::Flags kFlagsAnyTypeDeletedService = OT_SRP_SERVER_FLAGS_ANY_TYPE_DELETED_SERVICE;

    /**
     * This constructor initializes the SRP server object.
     *
     * @param[in]  aInstance  A reference to the OpenThread instance.
     *
     */
    explicit Server(Instance &aInstance);

    /**
     * This method sets the SRP service events handler.
     *
     * @param[in]  aServiceHandler         A service events handler.
     * @param[in]  aServiceHandlerContext  A pointer to arbitrary context information.
     *
     * @note  The handler SHOULD call HandleServiceUpdateResult to report the result of its processing.
     *        Otherwise, a SRP update will be considered failed.
     *
     * @sa  HandleServiceUpdateResult
     *
     */
    void SetServiceHandler(otSrpServerServiceUpdateHandler aServiceHandler, void *aServiceHandlerContext);

    /**
     * This method returns the domain authorized to the SRP server.
     *
     * If the domain if not set by SetDomain, "default.service.arpa." will be returned.
     * A trailing dot is always appended even if the domain is set without it.
     *
     * @returns A pointer to the dot-joined domain string.
     *
     */
    const char *GetDomain(void) const { return mDomain.AsCString(); }

    /**
     * This method sets the domain on the SRP server.
     *
     * A trailing dot will be appended to @p aDomain if it is not already there.
     * This method should only be called before the SRP server is enabled.
     *
     * @param[in]  aDomain  The domain to be set. MUST NOT be `nullptr`.
     *
     * @retval  kErrorNone          Successfully set the domain to @p aDomain.
     * @retval  kErrorInvalidState  The SRP server is already enabled and the Domain cannot be changed.
     * @retval  kErrorInvalidArgs   The argument @p aDomain is not a valid DNS domain name.
     * @retval  kErrorNoBufs        There is no memory to store content of @p aDomain.
     *
     */
    Error SetDomain(const char *aDomain);

    /**
     * This method returns the address mode being used by the SRP server.
     *
     * @returns The SRP server's address mode.
     *
     */
    AddressMode GetAddressMode(void) const { return mAddressMode; }

    /**
     * This method sets the address mode to be used by the SRP server.
     *
     * @param[in] aMode      The address mode to use.
     *
     * @retval kErrorNone           Successfully set the address mode.
     * @retval kErrorInvalidState   The SRP server is enabled and the address mode cannot be changed.
     *
     */
    Error SetAddressMode(AddressMode aMode);

    /**
     * This method gets the sequence number used with anycast address mode.
     *
     * The sequence number is included in "DNS/SRP Service Anycast Address" entry published in the Network Data.
     *
     * @returns The anycast sequence number.
     *
     */
    uint8_t GetAnycastModeSequenceNumber(void) const { return mAnycastSequenceNumber; }

    /**
     * This method sets the sequence number used with anycast address mode.
     *
     * @param[in] aSequenceNumber  The sequence number to use.
     *
     * @retval kErrorNone           Successfully set the address mode.
     * @retval kErrorInvalidState   The SRP server is enabled and the sequence number cannot be changed.
     *
     */
    Error SetAnycastModeSequenceNumber(uint8_t aSequenceNumber);

    /**
     * This method returns the state of the SRP server.
     *
     * @returns  The state of the server.
     *
     */
    State GetState(void) const { return mState; }

    /**
     * This method tells the port the SRP server is listening to.
     *
     * @returns  The port of the server or 0 if the SRP server is not running.
     *
     */
    uint16_t GetPort(void) const { return (mState == kStateRunning) ? mPort : 0; }

    /**
     * This method enables/disables the SRP server.
     *
     * @param[in]  aEnabled  A boolean to enable/disable the SRP server.
     *
     */
    void SetEnabled(bool aEnabled);

#if OPENTHREAD_CONFIG_BORDER_ROUTING_ENABLE
    /**
     * This method enables/disables the auto-enable mode on SRP server.
     *
     * When this mode is enabled, the Border Routing Manager controls if/when to enable or disable the SRP server.
     * SRP sever is auto-enabled if/when Border Routing is started it is done with the initial prefix and route
     * configurations (when the OMR and on-link prefixes are determined, advertised in emitted Router Advert message on
     * infrastructure side and published in the Thread Network Data). The SRP server is auto-disabled when BR is
     * stopped (e.g., if the infrastructure network interface is brought down or if BR gets detached).
     *
     * This mode can be disabled by a `SetAutoEnableMode(false)` call or if the SRP server is explicitly enabled or
     * disabled by a call to `SetEnabled()` method. Disabling auto-enable mode using `SetAutoEnableMode(false` call
     * will not change the current state of SRP sever (e.g., if it is enabled it stays enabled).
     *
     * @param[in] aEnbaled    A boolean to enable/disable the auto-enable mode.
     *
     */
    void SetAutoEnableMode(bool aEnabled);

    /**
     * This method indicates whether the auto-enable mode is enabled or disabled.
     *
     * @retval TRUE   The auto-enable mode is enabled.
     * @retval FALSE  The auto-enable mode is disabled.
     *
     */
    bool IsAutoEnableMode(void) const { return mAutoEnable; }
#endif

    /**
     * This method returns the TTL configuration.
     *
     * @param[out]  aTtlConfig  A reference to the `TtlConfig` instance.
     *
     */
    void GetTtlConfig(TtlConfig &aTtlConfig) const { aTtlConfig = mTtlConfig; }

    /**
     * This method sets the TTL configuration.
     *
     * @param[in]  aTtlConfig  A reference to the `TtlConfig` instance.
     *
     * @retval  kErrorNone         Successfully set the TTL configuration
     * @retval  kErrorInvalidArgs  The TTL range is not valid.
     *
     */
    Error SetTtlConfig(const TtlConfig &aTtlConfig);

    /**
     * This method returns the LEASE and KEY-LEASE configurations.
     *
     * @param[out]  aLeaseConfig  A reference to the `LeaseConfig` instance.
     *
     */
    void GetLeaseConfig(LeaseConfig &aLeaseConfig) const { aLeaseConfig = mLeaseConfig; }

    /**
     * This method sets the LEASE and KEY-LEASE configurations.
     *
     * When a LEASE time is requested from a client, the granted value will be
     * limited in range [aMinLease, aMaxLease]; and a KEY-LEASE will be granted
     * in range [aMinKeyLease, aMaxKeyLease].
     *
     * @param[in]  aLeaseConfig  A reference to the `LeaseConfig` instance.
     *
     * @retval  kErrorNone         Successfully set the LEASE and KEY-LEASE ranges.
     * @retval  kErrorInvalidArgs  The LEASE or KEY-LEASE range is not valid.
     *
     */
    Error SetLeaseConfig(const LeaseConfig &aLeaseConfig);

    /**
     * This method returns the next registered SRP host.
     *
     * @param[in]  aHost  The current SRP host; use `nullptr` to get the first SRP host.
     *
     * @returns  A pointer to the next SRP host or `nullptr` if no more SRP hosts can be found.
     *
     */
    const Host *GetNextHost(const Host *aHost);

    /**
     * This method returns the response counters of the SRP server.
     *
     * @returns  A pointer to the response counters of the SRP server.
     *
     */
    const otSrpServerResponseCounters *GetResponseCounters(void) const { return &mResponseCounters; }

    /**
     * This method receives the service update result from service handler set by
     * SetServiceHandler.
     *
     * @param[in]  aId     The ID of the service update transaction.
     * @param[in]  aError  The service update result.
     *
     */
    void HandleServiceUpdateResult(ServiceUpdateId aId, Error aError);

private:
    static constexpr uint16_t kUdpPayloadSize = Ip6::kMaxDatagramLength - sizeof(Ip6::Udp::Header);

    static constexpr uint32_t kDefaultMinLease             = 30;          // 30 seconds.
    static constexpr uint32_t kDefaultMaxLease             = 27u * 3600;  // 27 hours (in seconds).
    static constexpr uint32_t kDefaultMinKeyLease          = 30;          // 30 seconds.
    static constexpr uint32_t kDefaultMaxKeyLease          = 189u * 3600; // 189 hours (in seconds).
    static constexpr uint32_t kDefaultMinTtl               = kDefaultMinLease;
    static constexpr uint32_t kDefaultMaxTtl               = kDefaultMaxLease;
    static constexpr uint32_t kDefaultEventsHandlerTimeout = OPENTHREAD_CONFIG_SRP_SERVER_SERVICE_UPDATE_TIMEOUT;

    static constexpr AddressMode kDefaultAddressMode =
        static_cast<AddressMode>(OPENTHREAD_CONFIG_SRP_SERVER_DEFAULT_ADDDRESS_MODE);

    static constexpr uint16_t kAnycastAddressModePort = 53;

    // Metdata for a received SRP Update message.
    struct MessageMetadata
    {
        // Indicates whether the `Message` is received directly from a
        // client or from an SRPL partner.
        bool IsDirectRxFromClient(void) const { return (mMessageInfo != nullptr); }

        Dns::UpdateHeader       mDnsHeader;
        Dns::Zone               mDnsZone;
        uint16_t                mOffset;
        TimeMilli               mRxTime;
        TtlConfig               mTtlConfig;
        LeaseConfig             mLeaseConfig;
        const Ip6::MessageInfo *mMessageInfo; // Set to `nullptr` when from SRPL.
    };

    // This class includes metadata for processing a SRP update (register, deregister)
    // and sending DNS response to the client.
    class UpdateMetadata : public InstanceLocator,
                           public LinkedListEntry<UpdateMetadata>,
                           public Heap::Allocatable<UpdateMetadata>
    {
        friend class LinkedListEntry<UpdateMetadata>;
        friend class Heap::Allocatable<UpdateMetadata>;

    public:
        TimeMilli                GetExpireTime(void) const { return mExpireTime; }
        const Dns::UpdateHeader &GetDnsHeader(void) const { return mDnsHeader; }
        ServiceUpdateId          GetId(void) const { return mId; }
        const TtlConfig         &GetTtlConfig(void) const { return mTtlConfig; }
        const LeaseConfig       &GetLeaseConfig(void) const { return mLeaseConfig; }
        Host                    &GetHost(void) { return mHost; }
        const Ip6::MessageInfo  &GetMessageInfo(void) const { return mMessageInfo; }
        bool                     IsDirectRxFromClient(void) const { return mIsDirectRxFromClient; }
        bool                     Matches(ServiceUpdateId aId) const { return mId == aId; }

    private:
        UpdateMetadata(Instance &aInstance, Host &aHost, const MessageMetadata &aMessageMetadata);

        UpdateMetadata   *mNext;
        TimeMilli         mExpireTime;
        Dns::UpdateHeader mDnsHeader;
        ServiceUpdateId   mId;          // The ID of this service update transaction.
        TtlConfig         mTtlConfig;   // TTL config to use when processing the message.
        LeaseConfig       mLeaseConfig; // Lease config to use when processing the message.
        Host             &mHost;        // The `UpdateMetadata` has no ownership of this host.
        Ip6::MessageInfo  mMessageInfo; // Valid when `mIsDirectRxFromClient` is true.
        bool              mIsDirectRxFromClient;
    };

    void              Enable(void);
    void              Disable(void);
    void              Start(void);
    void              Stop(void);
    void              SelectPort(void);
    void              PrepareSocket(void);
    Ip6::Udp::Socket &GetSocket(void);

#if OPENTHREAD_CONFIG_DNSSD_SERVER_ENABLE
    void  HandleDnssdServerStateChange(void);
    Error HandleDnssdServerUdpReceive(Message &aMessage, const Ip6::MessageInfo &aMessageInfo);
#endif

    void HandleNetDataPublisherEvent(NetworkData::Publisher::Event aEvent);

    ServiceUpdateId AllocateId(void) { return mServiceUpdateId++; }

    void  InformUpdateHandlerOrCommit(Error aError, Host &aHost, const MessageMetadata &aMetadata);
    void  CommitSrpUpdate(Error aError, Host &aHost, const MessageMetadata &aMessageMetadata);
    void  CommitSrpUpdate(Error aError, UpdateMetadata &aUpdateMetadata);
    void  CommitSrpUpdate(Error                    aError,
                          Host                    &aHost,
                          const Dns::UpdateHeader &aDnsHeader,
                          const Ip6::MessageInfo  *aMessageInfo,
                          const TtlConfig         &aTtlConfig,
                          const LeaseConfig       &aLeaseConfig);
    Error ProcessMessage(Message &aMessage, const Ip6::MessageInfo &aMessageInfo);
    Error ProcessMessage(Message                &aMessage,
                         TimeMilli               aRxTime,
                         const TtlConfig        &aTtlConfig,
                         const LeaseConfig      &aLeaseConfig,
                         const Ip6::MessageInfo *aMessageInfo);
    void  ProcessDnsUpdate(Message &aMessage, MessageMetadata &aMetadata);
    Error ProcessUpdateSection(Host &aHost, const Message &aMessage, MessageMetadata &aMetadata) const;
    Error ProcessAdditionalSection(Host *aHost, const Message &aMessage, MessageMetadata &aMetadata) const;
    Error VerifySignature(const Dns::Ecdsa256KeyRecord &aKeyRecord,
                          const Message                &aMessage,
                          Dns::UpdateHeader             aDnsHeader,
                          uint16_t                      aSigOffset,
                          uint16_t                      aSigRdataOffset,
                          uint16_t                      aSigRdataLength,
                          const char                   *aSignerName) const;
    Error ValidateServiceSubTypes(Host &aHost, const MessageMetadata &aMetadata);
    Error ProcessZoneSection(const Message &aMessage, MessageMetadata &aMetadata) const;
    Error ProcessHostDescriptionInstruction(Host                  &aHost,
                                            const Message         &aMessage,
                                            const MessageMetadata &aMetadata) const;
    Error ProcessServiceDiscoveryInstructions(Host                  &aHost,
                                              const Message         &aMessage,
                                              const MessageMetadata &aMetadata) const;
    Error ProcessServiceDescriptionInstructions(Host &aHost, const Message &aMessage, MessageMetadata &aMetadata) const;

    static bool IsValidDeleteAllRecord(const Dns::ResourceRecord &aRecord);

    void        HandleUpdate(Host &aHost, const MessageMetadata &aMetadata);
    void        AddHost(Host &aHost);
    void        RemoveHost(Host *aHost, RetainName aRetainName, NotifyMode aNotifyServiceHandler);
    bool        HasNameConflictsWith(Host &aHost) const;
    void        SendResponse(const Dns::UpdateHeader    &aHeader,
                             Dns::UpdateHeader::Response aResponseCode,
                             const Ip6::MessageInfo     &aMessageInfo);
    void        SendResponse(const Dns::UpdateHeader &aHeader,
                             uint32_t                 aLease,
                             uint32_t                 aKeyLease,
                             const Ip6::MessageInfo  &aMessageInfo);
    static void HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);
    void        HandleUdpReceive(Message &aMessage, const Ip6::MessageInfo &aMessageInfo);
    void        HandleLeaseTimer(void);
    static void HandleOutstandingUpdatesTimer(Timer &aTimer);
    void        HandleOutstandingUpdatesTimer(void);

    void                  HandleServiceUpdateResult(UpdateMetadata *aUpdate, Error aError);
    const UpdateMetadata *FindOutstandingUpdate(const MessageMetadata &aMessageMetadata) const;
    static const char    *AddressModeToString(AddressMode aMode);

    void UpdateResponseCounters(Dns::Header::Response aResponseCode);

    using LeaseTimer  = TimerMilliIn<Server, &Server::HandleLeaseTimer>;
    using UpdateTimer = TimerMilliIn<Server, &Server::HandleOutstandingUpdatesTimer>;

    Ip6::Udp::Socket                mSocket;
    otSrpServerServiceUpdateHandler mServiceUpdateHandler;
    void                           *mServiceUpdateHandlerContext;

    Heap::String mDomain;

    TtlConfig   mTtlConfig;
    LeaseConfig mLeaseConfig;

    LinkedList<Host> mHosts;
    LeaseTimer       mLeaseTimer;

    UpdateTimer                mOutstandingUpdatesTimer;
    LinkedList<UpdateMetadata> mOutstandingUpdates;

    ServiceUpdateId mServiceUpdateId;
    uint16_t        mPort;
    State           mState;
    AddressMode     mAddressMode;
    uint8_t         mAnycastSequenceNumber;
    bool            mHasRegisteredAnyService : 1;
#if OPENTHREAD_CONFIG_BORDER_ROUTING_ENABLE
    bool mAutoEnable : 1;
#endif

    otSrpServerResponseCounters mResponseCounters;
};

} // namespace Srp

DefineCoreType(otSrpServerTtlConfig, Srp::Server::TtlConfig);
DefineCoreType(otSrpServerLeaseConfig, Srp::Server::LeaseConfig);
DefineCoreType(otSrpServerHost, Srp::Server::Host);
DefineCoreType(otSrpServerService, Srp::Server::Service);
DefineMapEnum(otSrpServerState, Srp::Server::State);
DefineMapEnum(otSrpServerAddressMode, Srp::Server::AddressMode);

} // namespace ot

#endif // OPENTHREAD_CONFIG_SRP_SERVER_ENABLE
#endif // NET_SRP_SERVER_HPP_

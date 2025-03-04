/*
 *  Copyright (c) 2021, The OpenThread Authors.
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
 *   This file contains definitions for the CLI output.
 */

#ifndef CLI_OUTPUT_HPP_
#define CLI_OUTPUT_HPP_

#include "openthread-core-config.h"

#include <stdarg.h>

#include <openthread/cli.h>

#include "cli_config.h"

#include "common/binary_search.hpp"
#include "common/num_utils.hpp"
#include "common/string.hpp"
#include "utils/parse_cmdline.hpp"

namespace ot {
namespace Cli {

/**
 * This type represents a ID number value associated with a CLI command string.
 *
 */
typedef uint64_t CommandId;

/**
 * This `constexpr` function converts a CLI command string to its associated `CommandId` value.
 *
 * @param[in] aString   The CLI command string.
 *
 * @returns The associated `CommandId` with @p aString.
 *
 */
constexpr static CommandId Cmd(const char *aString)
{
    return (aString[0] == '\0') ? 0 : (static_cast<uint8_t>(aString[0]) + Cmd(aString + 1) * 255u);
}

class Output;

/**
 * This class implements the basic output functions.
 *
 */
class OutputImplementer
{
    friend class Output;

public:
    /**
     * This constructor initializes the `OutputImplementer` object.
     *
     * @param[in] aCallback           A pointer to an `otCliOutputCallback` to deliver strings to the CLI console.
     * @param[in] aCallbackContext    An arbitrary context to pass in when invoking @p aCallback.
     *
     */
    OutputImplementer(otCliOutputCallback aCallback, void *aCallbackContext);

#if OPENTHREAD_CONFIG_CLI_LOG_INPUT_OUTPUT_ENABLE
    void SetEmittingCommandOutput(bool aEmittingOutput) { mEmittingCommandOutput = aEmittingOutput; }
#else
    void SetEmittingCommandOutput(bool) {}
#endif

private:
    static constexpr uint16_t kInputOutputLogStringSize = OPENTHREAD_CONFIG_CLI_LOG_INPUT_OUTPUT_LOG_STRING_SIZE;

    void OutputV(const char *aFormat, va_list aArguments);

    otCliOutputCallback mCallback;
    void               *mCallbackContext;
#if OPENTHREAD_CONFIG_CLI_LOG_INPUT_OUTPUT_ENABLE
    char     mOutputString[kInputOutputLogStringSize];
    uint16_t mOutputLength;
    bool     mEmittingCommandOutput;
#endif
};

/**
 * This class provides CLI output helper methods.
 *
 */
class Output
{
public:
    typedef Utils::CmdLineParser::Arg Arg; ///< An argument

    /**
     * This structure represent a CLI command table entry, mapping a command with `aName` to a handler method.
     *
     * @tparam Cli    The CLI module type.
     *
     */
    template <typename Cli> struct CommandEntry
    {
        typedef otError (Cli::*Handler)(Arg aArgs[]); ///< The handler method pointer type.

        /**
         * This method compares the entry's name with a given name.
         *
         * @param aName    The name string to compare with.
         *
         * @return zero means perfect match, positive (> 0) indicates @p aName is larger than entry's name, and
         *         negative (< 0) indicates @p aName is smaller than entry's name.
         *
         */
        int Compare(const char *aName) const { return strcmp(aName, mName); }

        /**
         * This `constexpr` method compares two entries to check if they are in order.
         *
         * @param[in] aFirst     The first entry.
         * @param[in] aSecond    The second entry.
         *
         * @retval TRUE  if @p aFirst and @p aSecond are in order, i.e. `aFirst < aSecond`.
         * @retval FALSE if @p aFirst and @p aSecond are not in order, i.e. `aFirst >= aSecond`.
         *
         */
        constexpr static bool AreInOrder(const CommandEntry &aFirst, const CommandEntry &aSecond)
        {
            return AreStringsInOrder(aFirst.mName, aSecond.mName);
        }

        const char *mName;    ///< The command name.
        Handler     mHandler; ///< The handler method pointer.
    };

    static const char kUnknownString[]; // Constant string "unknown".

    /**
     * This template static method converts an enumeration value to a string using a table array.
     *
     * @tparam EnumType       The `enum` type.
     * @tparam kLength        The table array length (number of entries in the array).
     *
     * @param[in] aEnum       The enumeration value to convert (MUST be of `EnumType`).
     * @param[in] aTable      A reference to the array of strings of length @p kLength. `aTable[e]` is the string
     *                        representation of enumeration value `e`.
     * @param[in] aNotFound   The string to return if the @p aEnum is not in the @p aTable.
     *
     * @returns The string representation of @p aEnum from @p aTable, or @p aNotFound if it is not in the table.
     *
     */
    template <typename EnumType, uint16_t kLength>
    static const char *Stringify(EnumType aEnum,
                                 const char *const (&aTable)[kLength],
                                 const char *aNotFound = kUnknownString)
    {
        return (static_cast<uint16_t>(aEnum) < kLength) ? aTable[static_cast<uint16_t>(aEnum)] : aNotFound;
    }

    /**
     * This constructor initializes the `Output` object.
     *
     * @param[in] aInstance           A pointer to OpenThread instance.
     * @param[in] aImplementer        An `OutputImplementer`.
     *
     */
    Output(otInstance *aInstance, OutputImplementer &aImplementer)
        : mInstance(aInstance)
        , mImplementer(aImplementer)
    {
    }

    /**
     * This method returns the pointer to OpenThread instance.
     *
     * @returns The pointer to the OpenThread instance.
     *
     */
    otInstance *GetInstancePtr(void) { return mInstance; }

    /**
     * This structure represents a buffer which is sued when converting a `uint64` value to string in decimal format.
     *
     */
    struct Uint64StringBuffer
    {
        static constexpr uint16_t kSize = 21; ///< Size of a buffer

        char mChars[kSize]; ///< Char array (do not access the array directly).
    };

    /**
     * This static method converts a `uint64_t` value to a decimal format string.
     *
     * @param[in] aUint64  The `uint64_t` value to convert.
     * @param[in] aBuffer  A buffer to allocate the string from.
     *
     * @returns A pointer to the start of the string (null-terminated) representation of @p aUint64.
     *
     */
    static const char *Uint64ToString(uint64_t aUint64, Uint64StringBuffer &aBuffer);

    /**
     * This method delivers a formatted output string to the CLI console.
     *
     * @param[in]  aFormat  A pointer to the format string.
     * @param[in]  ...      A variable list of arguments to format.
     *
     */
    void OutputFormat(const char *aFormat, ...) OT_TOOL_PRINTF_STYLE_FORMAT_ARG_CHECK(2, 3);

    /**
     * This method delivers a formatted output string to the CLI console (to which it prepends a given number
     * indentation space chars).
     *
     * @param[in]  aIndentSize   Number of indentation space chars to prepend to the string.
     * @param[in]  aFormat       A pointer to the format string.
     * @param[in]  ...           A variable list of arguments to format.
     *
     */
    void OutputFormat(uint8_t aIndentSize, const char *aFormat, ...) OT_TOOL_PRINTF_STYLE_FORMAT_ARG_CHECK(3, 4);

    /**
     * This method delivers a formatted output string to the CLI console (to which it also appends newline "\r\n").
     *
     * @param[in]  aFormat  A pointer to the format string.
     * @param[in]  ...      A variable list of arguments to format.
     *
     */
    void OutputLine(const char *aFormat, ...) OT_TOOL_PRINTF_STYLE_FORMAT_ARG_CHECK(2, 3);

    /**
     * This method delivers a formatted output string to the CLI console (to which it prepends a given number
     * indentation space chars and appends newline "\r\n").
     *
     * @param[in]  aIndentSize   Number of indentation space chars to prepend to the string.
     * @param[in]  aFormat       A pointer to the format string.
     * @param[in]  ...           A variable list of arguments to format.
     *
     */
    void OutputLine(uint8_t aIndentSize, const char *aFormat, ...) OT_TOOL_PRINTF_STYLE_FORMAT_ARG_CHECK(3, 4);

    /**
     * This method delivered newline "\r\n" to the CLI console.
     *
     */
    void OutputNewLine(void);

    /**
     * This method outputs a given number of space chars to the CLI console.
     *
     * @param[in] aCount  Number of space chars to output.
     *
     */
    void OutputSpaces(uint8_t aCount);

    /**
     * This method outputs a number of bytes to the CLI console as a hex string.
     *
     * @param[in]  aBytes   A pointer to data which should be printed.
     * @param[in]  aLength  @p aBytes length.
     *
     */
    void OutputBytes(const uint8_t *aBytes, uint16_t aLength);

    /**
     * This method outputs a number of bytes to the CLI console as a hex string and at the end it also outputs newline
     * "\r\n".
     *
     * @param[in]  aBytes   A pointer to data which should be printed.
     * @param[in]  aLength  @p aBytes length.
     *
     */
    void OutputBytesLine(const uint8_t *aBytes, uint16_t aLength);

    /**
     * This method outputs a number of bytes to the CLI console as a hex string.
     *
     * @tparam kBytesLength   The length of @p aBytes array.
     *
     * @param[in]  aBytes     A array of @p kBytesLength bytes which should be printed.
     *
     */
    template <uint8_t kBytesLength> void OutputBytes(const uint8_t (&aBytes)[kBytesLength])
    {
        OutputBytes(aBytes, kBytesLength);
    }

    /**
     * This method outputs a number of bytes to the CLI console as a hex string and at the end it also outputs newline
     * "\r\n".
     *
     * @tparam kBytesLength   The length of @p aBytes array.
     *
     * @param[in]  aBytes     A array of @p kBytesLength bytes which should be printed.
     *
     */
    template <uint8_t kBytesLength> void OutputBytesLine(const uint8_t (&aBytes)[kBytesLength])
    {
        OutputBytesLine(aBytes, kBytesLength);
    }

    /**
     * This method outputs an Extended MAC Address to the CLI console.
     *
     * param[in] aExtAddress  The Extended MAC Address to output.
     *
     */
    void OutputExtAddress(const otExtAddress &aExtAddress) { OutputBytes(aExtAddress.m8); }

    /**
     * This method outputs an Extended MAC Address to the CLI console and at the end it also outputs newline "\r\n".
     *
     * param[in] aExtAddress  The Extended MAC Address to output.
     *
     */
    void OutputExtAddressLine(const otExtAddress &aExtAddress) { OutputBytesLine(aExtAddress.m8); }

    /**
     * This method outputs a `uint64_t` value in decimal format.
     *
     * @param[in] aUint64   The `uint64_t` value to output.
     *
     */
    void OutputUint64(uint64_t aUint64);

    /**
     * This method outputs a `uint64_t` value in decimal format and at the end it also outputs newline "\r\n".
     *
     * @param[in] aUint64   The `uint64_t` value to output.
     *
     */
    void OutputUint64Line(uint64_t aUint64);

    /**
     * This method outputs "Enabled" or "Disabled" status to the CLI console (it also appends newline "\r\n").
     *
     * @param[in] aEnabled  A boolean indicating the status. TRUE outputs "Enabled", FALSE outputs "Disabled".
     *
     */
    void OutputEnabledDisabledStatus(bool aEnabled);

#if OPENTHREAD_FTD || OPENTHREAD_MTD

    /**
     * This method outputs an IPv6 address to the CLI console.
     *
     * @param[in]  aAddress  A reference to the IPv6 address.
     *
     */
    void OutputIp6Address(const otIp6Address &aAddress);

    /**
     * This method outputs an IPv6 address to the CLI console and at the end it also outputs newline "\r\n".
     *
     * @param[in]  aAddress  A reference to the IPv6 address.
     *
     */
    void OutputIp6AddressLine(const otIp6Address &aAddress);

    /**
     * This method outputs an IPv6 prefix to the CLI console.
     *
     * @param[in]  aPrefix  A reference to the IPv6 prefix.
     *
     */
    void OutputIp6Prefix(const otIp6Prefix &aPrefix);

    /**
     * This method outputs an IPv6 prefix to the CLI console and at the end it also outputs newline "\r\n".
     *
     * @param[in]  aPrefix  A reference to the IPv6 prefix.
     *
     */
    void OutputIp6PrefixLine(const otIp6Prefix &aPrefix);

    /**
     * This method outputs an IPv6 network prefix to the CLI console.
     *
     * @param[in]  aPrefix  A reference to the IPv6 network prefix.
     *
     */
    void OutputIp6Prefix(const otIp6NetworkPrefix &aPrefix);

    /**
     * This method outputs an IPv6 network prefix to the CLI console and at the end it also outputs newline "\r\n".
     *
     * @param[in]  aPrefix  A reference to the IPv6 network prefix.
     *
     */
    void OutputIp6PrefixLine(const otIp6NetworkPrefix &aPrefix);

    /**
     * This method outputs an IPv6 socket address to the CLI console.
     *
     * @param[in] aSockAddr   A reference to the IPv6 socket address.
     *
     */
    void OutputSockAddr(const otSockAddr &aSockAddr);

    /**
     * This method outputs an IPv6 socket address to the CLI console and at the end it also outputs newline "\r\n".
     *
     * @param[in] aSockAddr   A reference to the IPv6 socket address.
     *
     */
    void OutputSockAddrLine(const otSockAddr &aSockAddr);

    /**
     * This method outputs DNS TXT data to the CLI console.
     *
     * @param[in] aTxtData        A pointer to a buffer containing the DNS TXT data.
     * @param[in] aTxtDataLength  The length of @p aTxtData (in bytes).
     *
     */
    void OutputDnsTxtData(const uint8_t *aTxtData, uint16_t aTxtDataLength);

#endif // OPENTHREAD_FTD || OPENTHREAD_MTD

    /**
     * This method outputs a table header to the CLI console.
     *
     * An example of the table header format:
     *
     *    | Title1    | Title2 |Title3| Title4               |
     *    +-----------+--------+------+----------------------+
     *
     * The titles are left adjusted (extra white space is added at beginning if the column is width enough). The widths
     * are specified as the number chars between two `|` chars (excluding the char `|` itself).
     *
     * @tparam kTableNumColumns   The number columns in the table.
     *
     * @param[in] aTitles   An array specifying the table column titles.
     * @param[in] aWidths   An array specifying the table column widths (in number of chars).
     *
     */
    template <uint8_t kTableNumColumns>
    void OutputTableHeader(const char *const (&aTitles)[kTableNumColumns], const uint8_t (&aWidths)[kTableNumColumns])
    {
        OutputTableHeader(kTableNumColumns, &aTitles[0], &aWidths[0]);
    }

    /**
     * This method outputs a table separator to the CLI console.
     *
     * An example of the table separator:
     *
     *    +-----------+--------+------+----------------------+
     *
     * The widths are specified as number chars between two `+` chars (excluding the char `+` itself).
     *
     * @tparam kTableNumColumns   The number columns in the table.
     *
     * @param[in] aWidths   An array specifying the table column widths (in number of chars).
     *
     */
    template <uint8_t kTableNumColumns> void OutputTableSeparator(const uint8_t (&aWidths)[kTableNumColumns])
    {
        OutputTableSeparator(kTableNumColumns, &aWidths[0]);
    }

    /**
     * This method outputs the list of commands from a given command table.
     *
     * @tparam Cli      The CLI module type.
     * @tparam kLength  The length of command table array.
     *
     * @param[in] aCommandTable   The command table array.
     *
     */
    template <typename Cli, uint16_t kLength> void OutputCommandTable(const CommandEntry<Cli> (&aCommandTable)[kLength])
    {
        for (const CommandEntry<Cli> &entry : aCommandTable)
        {
            OutputLine("%s", entry.mName);
        }
    }

protected:
    void OutputFormatV(const char *aFormat, va_list aArguments);

#if OPENTHREAD_CONFIG_CLI_LOG_INPUT_OUTPUT_ENABLE
    void LogInput(const Arg *aArgs);
#else
    void LogInput(const Arg *) {}
#endif

private:
    static constexpr uint16_t kInputOutputLogStringSize = OPENTHREAD_CONFIG_CLI_LOG_INPUT_OUTPUT_LOG_STRING_SIZE;

    void OutputTableHeader(uint8_t aNumColumns, const char *const aTitles[], const uint8_t aWidths[]);
    void OutputTableSeparator(uint8_t aNumColumns, const uint8_t aWidths[]);

    otInstance        *mInstance;
    OutputImplementer &mImplementer;
};

} // namespace Cli
} // namespace ot

#endif // CLI_OUTPUT_HPP_

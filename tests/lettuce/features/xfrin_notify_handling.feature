Feature: Xfrin incoming notify handling
    Tests for Xfrin incoming notify handling.

    Scenario: Handle incoming notify
    Given I have bind10 running with configuration xfrin/retransfer_master.conf with cmdctl port 47804 as master
    And wait for master stderr message BIND10_STARTED_CC
    And wait for master stderr message CMDCTL_STARTED
    And wait for master stderr message AUTH_SERVER_STARTED
    And wait for master stderr message XFROUT_STARTED
    And wait for master stderr message ZONEMGR_STARTED
    And wait for master stderr message STATS_STARTING

    And I have bind10 running with configuration xfrin/retransfer_slave_notify.conf
    And wait for bind10 stderr message BIND10_STARTED_CC
    And wait for bind10 stderr message CMDCTL_STARTED
    And wait for bind10 stderr message AUTH_SERVER_STARTED
    And wait for bind10 stderr message XFRIN_STARTED
    And wait for bind10 stderr message ZONEMGR_STARTED

    A query for www.example.org to [::1]:47806 should have rcode NXDOMAIN

    #
    # Test for statistics
    #
    # check for initial statistics
    #
    When I query statistics zones of bind10 module Xfrout with cmdctl port 47804
    last bindctl output should not contain "error"
    last bindctl output should not contain "example.org."
    Then the statistics counter notifyoutv4 for the zone _SERVER_ should be 0
    Then the statistics counter notifyoutv6 for the zone _SERVER_ should be 0
    Then the statistics counter xfrrej for the zone _SERVER_ should be 0
    Then the statistics counter xfrreqdone for the zone _SERVER_ should be 0

    When I query statistics ixfr_running of bind10 module Xfrout with cmdctl port 47804
    Then the statistics counter ixfr_running should be 0
    Then the statistics counter ixfr_running should be 0

    When I query statistics axfr_running of bind10 module Xfrout with cmdctl port 47804
    Then the statistics counter axfr_running should be 0
    Then the statistics counter axfr_running should be 0

    When I send bind10 with cmdctl port 47804 the command Xfrout notify example.org IN
    Then wait for new master stderr message XFROUT_NOTIFY_COMMAND
    Then wait for new bind10 stderr message AUTH_RECEIVED_NOTIFY
    Then wait for new bind10 stderr message ZONEMGR_RECEIVE_NOTIFY
    Then wait for new bind10 stderr message XFRIN_XFR_TRANSFER_STARTED
    Then wait for new bind10 stderr message XFRIN_TRANSFER_SUCCESS not XFRIN_XFR_PROCESS_FAILURE
    Then wait for new bind10 stderr message ZONEMGR_RECEIVE_XFRIN_SUCCESS
    Then wait 5 times for new master stderr message NOTIFY_OUT_SENDING_NOTIFY
    Then wait for new master stderr message NOTIFY_OUT_RETRY_EXCEEDED

    A query for www.example.org to [::1]:47806 should have rcode NOERROR

    #
    # Test for statistics
    #
    # check for statistics change
    #

    # wait until the last stats requesting is finished
    wait for new master stderr message STATS_SEND_STATISTICS_REQUEST
    wait for new master stderr message XFROUT_RECEIVED_GETSTATS_COMMAND

    When I query statistics zones of bind10 module Xfrout with cmdctl port 47804
    last bindctl output should not contain "error"
    Then the statistics counter notifyoutv4 for the zone _SERVER_ should be 0
    Then the statistics counter notifyoutv4 for the zone example.org. should be 0
    Then the statistics counter notifyoutv6 for the zone _SERVER_ should be 5
    Then the statistics counter notifyoutv6 for the zone example.org. should be 5
    Then the statistics counter xfrrej for the zone _SERVER_ should be 0
    Then the statistics counter xfrrej for the zone example.org. should be 0
    Then the statistics counter xfrreqdone for the zone _SERVER_ should be 1
    Then the statistics counter xfrreqdone for the zone example.org. should be 1

    #
    # Test for Xfr request rejected
    #
    Scenario: Handle incoming notify (XFR request rejected)
    Given I have bind10 running with configuration xfrin/retransfer_master.conf with cmdctl port 47804 as master
    And wait for master stderr message BIND10_STARTED_CC
    And wait for master stderr message CMDCTL_STARTED
    And wait for master stderr message AUTH_SERVER_STARTED
    And wait for master stderr message XFROUT_STARTED
    And wait for master stderr message ZONEMGR_STARTED
    And wait for master stderr message STATS_STARTING

    And I have bind10 running with configuration xfrin/retransfer_slave_notify.conf
    And wait for bind10 stderr message BIND10_STARTED_CC
    And wait for bind10 stderr message CMDCTL_STARTED
    And wait for bind10 stderr message AUTH_SERVER_STARTED
    And wait for bind10 stderr message XFRIN_STARTED
    And wait for bind10 stderr message ZONEMGR_STARTED

    A query for www.example.org to [::1]:47806 should have rcode NXDOMAIN

    #
    # Test1 for statistics
    #
    # check for initial statistics
    #
    When I query statistics zones of bind10 module Xfrout with cmdctl port 47804
    last bindctl output should not contain "error"
    last bindctl output should not contain "example.org."
    Then the statistics counter notifyoutv4 for the zone _SERVER_ should be 0
    Then the statistics counter notifyoutv6 for the zone _SERVER_ should be 0
    Then the statistics counter xfrrej for the zone _SERVER_ should be 0
    Then the statistics counter xfrreqdone for the zone _SERVER_ should be 0

    #
    # set transfer_acl rejection
    # Local xfr requests from Xfrin module would be rejected here.
    #
    When I send bind10 the following commands with cmdctl port 47804
    """
    config set Xfrout/zone_config[0]/transfer_acl [{"action":  "REJECT", "from": "::1"}]
    config commit
    """
    last bindctl output should not contain Error

    When I send bind10 with cmdctl port 47804 the command Xfrout notify example.org IN
    Then wait for new master stderr message XFROUT_NOTIFY_COMMAND
    Then wait for new bind10 stderr message AUTH_RECEIVED_NOTIFY
    Then wait for new bind10 stderr message ZONEMGR_RECEIVE_NOTIFY
    Then wait for new bind10 stderr message XFRIN_XFR_TRANSFER_STARTED
    Then wait for new bind10 stderr message XFRIN_XFR_TRANSFER_PROTOCOL_ERROR not XFRIN_XFR_TRANSFER_STARTED
    Then wait for new bind10 stderr message ZONEMGR_RECEIVE_XFRIN_FAILED not ZONEMGR_RECEIVE_XFRIN_SUCCESS
    Then wait 5 times for new master stderr message NOTIFY_OUT_SENDING_NOTIFY
    Then wait for new master stderr message NOTIFY_OUT_RETRY_EXCEEDED

    A query for www.example.org to [::1]:47806 should have rcode NXDOMAIN

    #
    # Test2 for statistics
    #
    # check for statistics change
    #

    # wait until the last stats requesting is finished
    wait for new master stderr message STATS_SEND_STATISTICS_REQUEST
    wait for new master stderr message XFROUT_RECEIVED_GETSTATS_COMMAND

    When I query statistics zones of bind10 module Xfrout with cmdctl port 47804
    last bindctl output should not contain "error"
    Then the statistics counter notifyoutv4 for the zone _SERVER_ should be 0
    Then the statistics counter notifyoutv4 for the zone example.org. should be 0
    Then the statistics counter notifyoutv6 for the zone _SERVER_ should be 5
    Then the statistics counter notifyoutv6 for the zone example.org. should be 5
    # The counts of rejection would be between 1 and 2. They are not
    # fixed. It would depend on timing or the platform.
    Then the statistics counter xfrrej for the zone _SERVER_ should be greater than 0
    Then the statistics counter xfrrej for the zone example.org. should be greater than 0
    Then the statistics counter xfrreqdone for the zone _SERVER_ should be 0
    Then the statistics counter xfrreqdone for the zone example.org. should be 0

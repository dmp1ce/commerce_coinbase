<?php

/**
 * @file
 * Transaction callbacks from Coinbase.
 */

/**
 * Process payment information from Coinbase.
 *
 * @link https://coinbase.com/docs/merchant_tools/callbacks Callbacks @endlink
 */
function commerce_coinbase_process_callback($order, $transaction) {
  /*
  At this point, the menu handler should have already checked the validity
  of the secret hash in the URL against the transaction. The data processed
  in this function should be considered safe if you trust the security of
  SSL on your callback URL.
  */
  watchdog('commerce_coinbase', 'Callback received for order @order_id, transaction @trans_id.', array('@trans_id' => $transaction->transaction_id, '@order_id' => $order->order_id), WATCHDOG_DEBUG, l(t('view order payments'), 'admin/commerce/orders/' . $order->order_id . '/payment'));

  $post_body = json_decode(file_get_contents('php://input'));
  if (empty($post_body)) {
    watchdog('commerce_coinbase', 'Transaction callback was empty.');
    return;
  }

  /*
  Even though everything should be trustworthy, requesting the information back
  from Coinbase makes extra sure we're getting a legitimate callback.
  */
  $payment_method = commerce_payment_method_instance_load($transaction->instance_id);
  libraries_load('coinbase-php');
  $coinbase = new Coinbase($payment_method['settings']['api_key']);
  $coinbase_order_info = $coinbase->getOrder($post_body->order->id);

  if (empty($coinbase_order_info->status)) {
    watchdog('commerce_coinbase', 'Invalid transaction callback.');
  }
  else {
    switch ($coinbase_order_info->status) {
      case 'completed':
        commerce_checkout_complete($order);
        $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
        $transaction->message = t('Merchant account credited. Invoice complete.');
        $transaction->remote_id = $coinbase_order_info->id;
        $transaction->remote_status = $coinbase_order_info->status;
        $transaction->payload = $coinbase_order_info;
        break;

      case 'canceled':
        $transaction->status = COMMERCE_PAYMENT_STATUS_FAILURE;
        $transaction->message = t('Payment was canceled.');
        $transaction->remote_status = $coinbase_order_info->status;
        $transaction->payload = $coinbase_order_info;
        break;

      default:
        watchdog('commerce_coinbase', 'Unknown payment status for transaction @id: @status', array('@id' => $transaction->transaction_id, '@status' => $coinbase_order_info->status), WATCHDOG_WARNING);
    }
    commerce_payment_transaction_save($transaction);
  }
}

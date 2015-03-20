<?php
/**
 * @file
 * Allow S3 credentials to be inherited from the client.
 */

/**
 * Implementation of hook_hosting_feature().
 */
function hosting_s3_client_creds_hosting_feature() {
  $features['s3_client_creds'] = array(
    'title' => t('S3 integration - Inherit client credentials'),
    'description' => t('Allow sites to inherit S3 credentials from the client\'s account.'),
    'status' => HOSTING_FEATURE_DISABLED,
    'module' => 'hosting_s3_client_creds',
    'group' => 'experimental'
  );
  return $features;
}

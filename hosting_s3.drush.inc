<?php

/**
 * @file
 * Drush integration for the S3 service.
 */

/**
 * Implements hook_hosting_site_context_options().
 */
function hosting_s3_hosting_site_context_options(&$task) {
  $fields = array('s3_access_key_id', 's3_secret_access_key');
  foreach ($fields as $field) {
    if (isset($task->ref->$field) && !empty($task->ref->$field)) {
      $task->context_options[$field] = $task->ref->$field;
    }
    else {
      $task->context_options[$field] = 'null';
    }
  }
}

/**
 * Implements hook_drush_context_import().
 */
function hosting_s3_drush_context_import($context, &$node) {
  if ($context->type == 'site') {
    $fields = array('s3_access_key_id', 's3_secret_access_key', 's3_bucket_name');
    foreach ($fields as $field) {
      if (isset($context->$field) && !empty($context->$field)) {
        $node->$field = $context->$field;
      }
    }
  }
}

/**
 * Implements hook_post_hosting_TASK_TYPE_task().
 */
function hosting_s3_post_hosting_install_task($task, $data) {
  if ($task->ref->type == 'site' && isset($data['context']['s3_bucket_name'])) {
    // Save the generated bucket name in the site node.
    // See: Provision_Service_s3::validate_bucket_name().
    $task->ref->s3_bucket_name = $data['context']['s3_bucket_name'];
  }
}

/**
 * Implements hook_post_hosting_TASK_TYPE_task().
 */
function hosting_s3_post_hosting_backup_task($task, $data) {
  if (isset($data['context']['s3_backup_name'])) {
    // Save the backup bucket name associated with the backup ID.
    // See: Provision_Service_s3::pre_backup().
    $bid = hosting_s3_get_latest_site_backup($task->ref->nid);
    $bucket = $data['context']['s3_backup_name'];
    hosting_s3_save_backup($bid, $bucket);
  }
}

/**
 * Look up the most recent backup for a given site.
 */
function hosting_s3_get_latest_site_backup($nid) {
  return db_query("SELECT bid FROM {hosting_site_backups} WHERE site = :site ORDER BY timestamp DESC LIMIT 1", array(':site' => $nid))->fetchField();
}

function hosting_s3_save_backup($bid, $bucket) {
  db_insert('hosting_s3_backups')->fields(array(
    'bid' => $bid,
    'bucket' => $bucket,
  ))->execute();
}

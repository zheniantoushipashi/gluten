/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package io.glutenproject.sql.shims

import io.glutenproject.expression.Sig

import org.apache.spark.sql.SparkSession
import org.apache.spark.sql.catalyst.InternalRow
import org.apache.spark.sql.catalyst.catalog.BucketSpec
import org.apache.spark.sql.catalyst.expressions.Expression
import org.apache.spark.sql.catalyst.plans.physical.Distribution
import org.apache.spark.sql.connector.expressions.Transform
import org.apache.spark.sql.execution.FileSourceScanExec
import org.apache.spark.sql.execution.datasources.{FilePartition, FileScanRDD, PartitionedFile}

sealed abstract class ShimDescriptor

case class SparkShimDescriptor(major: Int, minor: Int, patch: Int) extends ShimDescriptor {
  override def toString(): String = s"$major.$minor.$patch"

  def toMajorMinorVersion: String = s"$major.$minor"
}

trait SparkShims {
  def getShimDescriptor: ShimDescriptor

  // for this purpose, change HashClusteredDistribution to ClusteredDistribution
  // https://github.com/apache/spark/pull/32875
  def getDistribution(leftKeys: Seq[Expression], rightKeys: Seq[Expression]): Seq[Distribution]

  def expressionMappings: Seq[Sig]

  def convertPartitionTransforms(partitions: Seq[Transform]): (Seq[String], Option[BucketSpec])

  def generateFileScanRDD(
      sparkSession: SparkSession,
      readFunction: (PartitionedFile) => Iterator[InternalRow],
      filePartitions: Seq[FilePartition],
      fileSourceScanExec: FileSourceScanExec): FileScanRDD
}
